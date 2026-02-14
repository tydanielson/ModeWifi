# Cognito User Pool for email/password authentication
resource "aws_cognito_user_pool" "van_users" {
  name = "${var.thing_name}-users"

  # Invite-only: Only users you create in Cognito console can access
  admin_create_user_config {
    allow_admin_create_user_only = true
    invite_message_template {
      email_subject = "Your ${var.thing_name} van access"
      email_message = "You've been invited to access the van monitoring system. Username: {username}, Temporary password: {####}. You'll be prompted to change this on first login."
      sms_message   = "Your username is {username} and temporary password is {####}"
    }
  }

  # Email configuration
  auto_verified_attributes = ["email"]
  
  # Username attributes - allow sign in with email
  username_attributes = ["email"]
  
  # Password policy
  password_policy {
    minimum_length    = 8
    require_lowercase = true
    require_numbers   = true
    require_symbols   = false
    require_uppercase = true
    temporary_password_validity_days = 7
  }

  # Account recovery
  account_recovery_setting {
    recovery_mechanism {
      name     = "verified_email"
      priority = 1
    }
  }

  # User attributes
  schema {
    name                = "email"
    attribute_data_type = "String"
    required           = true
    mutable            = true
  }

  tags = {
    Name        = "${var.thing_name}-user-pool"
    Environment = "production"
  }
}

# Cognito User Pool Domain (for hosted UI)
resource "aws_cognito_user_pool_domain" "van_auth" {
  domain       = "${var.thing_name}-auth"
  user_pool_id = aws_cognito_user_pool.van_users.id
}

# Cognito UI Customization
resource "aws_cognito_user_pool_ui_customization" "van_auth_ui" {
  user_pool_id = aws_cognito_user_pool.van_users.id
  client_id    = aws_cognito_user_pool_client.van_dashboard.id

  css = <<CSS
/* Modern dark theme for van dashboard login */
.banner-customizable {
  background: linear-gradient(135deg, #1a1a2e 0%, #16213e 100%);
  padding: 30px 0;
}

.logo-customizable {
  max-width: 80px;
  max-height: 80px;
}

.textDescription-customizable {
  color: #e8e8e8;
  font-size: 16px;
  margin-top: 10px;
}

.idpButton-customizable {
  height: 50px;
  width: 100%;
  border-radius: 8px;
  background: linear-gradient(135deg, #0f3460 0%, #16213e 100%);
  border: 2px solid #e94560;
  color: #ffffff;
  font-size: 16px;
  font-weight: 600;
}

.legalText-customizable {
  color: #9a9a9a;
  font-size: 12px;
}

.submitButton-customizable {
  background: linear-gradient(135deg, #e94560 0%, #ff6b8a 100%);
  border: none;
  border-radius: 8px;
  color: white;
  height: 50px;
  font-size: 16px;
  font-weight: 600;
}

.inputField-customizable {
  border-radius: 8px;
  border: 2px solid #2d3748;
  background: #1a202c;
  color: #ffffff;
  height: 50px;
  padding: 0 15px;
  font-size: 15px;
}

.inputField-customizable:focus {
  border-color: #e94560;
  background: #2d3748;
  outline: none;
  box-shadow: 0 0 0 3px rgba(233, 69, 96, 0.1);
}

.label-customizable {
  color: #e8e8e8;
  font-weight: 600;
  font-size: 14px;
  margin-bottom: 8px;
}

.background-customizable {
  background: linear-gradient(135deg, #0f0c29 0%, #302b63 50%, #24243e 100%);
  min-height: 100vh;
}

.errorMessage-customizable {
  background-color: rgba(233, 69, 96, 0.1);
  border-left: 4px solid #e94560;
  color: #ff6b8a;
  padding: 12px;
  border-radius: 4px;
  margin: 10px 0;
}

.redirect-customizable {
  color: #64b5f6;
  text-decoration: none;
}
CSS

  depends_on = [aws_cognito_user_pool_domain.van_auth]
}

# Cognito User Pool Client (for web application)
resource "aws_cognito_user_pool_client" "van_dashboard" {
  name         = "${var.thing_name}-dashboard-client"
  user_pool_id = aws_cognito_user_pool.van_users.id

  # OAuth configuration for hosted UI
  allowed_oauth_flows_user_pool_client = true
  allowed_oauth_flows                  = ["code"]
  allowed_oauth_scopes                 = ["email", "openid", "profile"]
  
  # Callback URLs (CloudFront domain + optional custom domain)
  callback_urls = concat(
    [
      "https://${aws_cloudfront_distribution.webapp.domain_name}/callback.html",
      "http://localhost:8000/callback.html"  # For local development
    ],
    var.custom_domain != "" ? ["https://${var.custom_domain}/callback.html"] : []
  )
  
  logout_urls = concat(
    [
      "https://${aws_cloudfront_distribution.webapp.domain_name}",
      "http://localhost:8000"
    ],
    var.custom_domain != "" ? ["https://${var.custom_domain}"] : []
  )

  # Supported identity providers - use Cognito built-in
  supported_identity_providers = ["COGNITO"]

  # Token validity - Max allowed by AWS (24 hours) with auto-refresh
  # Van data is low-sensitivity, so maximize token lifetime
  # Refresh token handles automatic renewal for 90-day sessions
  id_token_validity      = 1440 # 24 hours (max allowed)
  access_token_validity  = 1440 # 24 hours (max allowed)
  refresh_token_validity = 90   # 90 days

  token_validity_units {
    id_token      = "minutes"
    access_token  = "minutes"
    refresh_token = "days"
  }

  # Don't generate a client secret (SPA)
  generate_secret = false

  # Prevent user existence errors
  prevent_user_existence_errors = "ENABLED"

  # Explicit auth flows
  explicit_auth_flows = [
    "ALLOW_REFRESH_TOKEN_AUTH",
    "ALLOW_USER_SRP_AUTH"
  ]

  # Read/write attributes
  read_attributes = [
    "email",
    "email_verified",
    "name"
  ]

  write_attributes = [
    "email",
    "name"
  ]
}

# Outputs
output "cognito_user_pool_id" {
  value       = aws_cognito_user_pool.van_users.id
  description = "Cognito User Pool ID"
}

output "cognito_client_id" {
  value       = aws_cognito_user_pool_client.van_dashboard.id
  description = "Cognito Client ID for dashboard"
}

output "cognito_hosted_ui_url" {
  value       = "https://${aws_cognito_user_pool_domain.van_auth.domain}.auth.${data.aws_region.current.name}.amazoncognito.com"
  description = "Cognito Hosted UI URL"
}

output "cognito_login_url" {
  value = "https://${aws_cognito_user_pool_domain.van_auth.domain}.auth.${data.aws_region.current.name}.amazoncognito.com/login?client_id=${aws_cognito_user_pool_client.van_dashboard.id}&response_type=code&scope=email+openid+profile&redirect_uri=${urlencode("https://${aws_cloudfront_distribution.webapp.domain_name}/callback.html")}"
  description = "Direct login URL for dashboard"
}
