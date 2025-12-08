# Cognito User Pool for invite-only Google authentication
resource "aws_cognito_user_pool" "van_users" {
  name = "${var.thing_name}-users"

  # Invite-only: Only users you create in Cognito console can access
  # Create users with their Google email addresses, they'll use Google to login
  admin_create_user_config {
    allow_admin_create_user_only = true
    invite_message_template {
      email_subject = "Your ${var.thing_name} van access"
      email_message = "You've been invited to access the van monitoring system. Use your Google account (this email) to log in at the dashboard."
      sms_message   = "You have access to the van monitoring system"
    }
  }

  # Email configuration
  auto_verified_attributes = ["email"]
  
  # Password policy (even though we're using Google, this applies to temporary passwords)
  password_policy {
    minimum_length    = 8
    require_lowercase = true
    require_numbers   = true
    require_symbols   = false
    require_uppercase = true
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

# Google Identity Provider
resource "aws_cognito_identity_provider" "google" {
  user_pool_id  = aws_cognito_user_pool.van_users.id
  provider_name = "Google"
  provider_type = "Google"

  provider_details = {
    authorize_scopes = "email openid profile"
    client_id        = var.google_client_id
    client_secret    = var.google_client_secret
  }

  attribute_mapping = {
    email    = "email"
    username = "sub"
    name     = "name"
  }
}

# Cognito User Pool Client (for web application)
resource "aws_cognito_user_pool_client" "van_dashboard" {
  name         = "${var.thing_name}-dashboard-client"
  user_pool_id = aws_cognito_user_pool.van_users.id

  # OAuth configuration for hosted UI
  allowed_oauth_flows_user_pool_client = true
  allowed_oauth_flows                  = ["code"]
  allowed_oauth_scopes                 = ["email", "openid", "profile"]
  
  # Callback URLs (CloudFront domain)
  callback_urls = [
    "https://${aws_cloudfront_distribution.webapp.domain_name}/callback.html",
    "http://localhost:8000/callback.html"  # For local development
  ]
  
  logout_urls = [
    "https://${aws_cloudfront_distribution.webapp.domain_name}",
    "http://localhost:8000"
  ]

  # Supported identity providers
  supported_identity_providers = ["Google"]

  # Token validity
  id_token_validity      = 60  # 60 minutes
  access_token_validity  = 60  # 60 minutes
  refresh_token_validity = 30  # 30 days

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
