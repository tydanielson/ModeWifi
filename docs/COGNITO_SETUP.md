# Cognito Authentication Setup Guide

This guide walks through setting up invite-only authentication for your van dashboard using AWS Cognito and Google OAuth.

## Overview

The dashboard now requires authentication via Google login. Only users you explicitly invite in the AWS Cognito console will have access.

**Security Model:**
- Invite-only (admin creates users)
- Google OAuth 2.0 for authentication
- JWT tokens protect API Gateway endpoints
- Session-based tokens (60-minute expiry)

## Prerequisites

- AWS account with Terraform infrastructure deployed
- Google account for OAuth setup
- Access to Google Cloud Console

## Step 1: Create Google OAuth Credentials

### 1.1 Access Google Cloud Console

1. Go to [Google Cloud Console](https://console.cloud.google.com/)
2. Create a new project or select an existing one
3. Navigate to **APIs & Services** → **Credentials**

### 1.2 Create OAuth 2.0 Client ID

1. Click **+ CREATE CREDENTIALS** → **OAuth client ID**
2. If prompted, configure the OAuth consent screen:
   - **User Type:** External (for personal Google accounts)
   - **App name:** Your van name (e.g., "Storyteller Van Monitor")
   - **User support email:** Your email
   - **Developer contact:** Your email
   - **Scopes:** Add `email`, `profile`, `openid` (or just use defaults)
   - **Test users:** Add your Google email address
   - Click **Save and Continue**

3. Back on the Credentials page, click **+ CREATE CREDENTIALS** → **OAuth client ID**
4. **Application type:** Web application
5. **Name:** "Van Dashboard - Cognito"

6. **Authorized JavaScript origins:**
   - Leave empty (not needed for our flow)

7. **Authorized redirect URIs:**
   - `https://<thing-name>-auth.auth.<region>.amazoncognito.com/oauth2/idpresponse`
   - Replace `<thing-name>` with your van's thing_name (e.g., `storyteller-van-01`)
   - Replace `<region>` with your AWS region (e.g., `us-east-1`)
   - Example: `https://storyteller-van-01-auth.auth.us-east-1.amazoncognito.com/oauth2/idpresponse`
   - Also add: `http://localhost:8000/callback.html` (for local testing)

8. Click **CREATE**

9. **Save the credentials:**
   - Copy the **Client ID** (looks like `xxxxx.apps.googleusercontent.com`)
   - Copy the **Client secret** (random string)
   - You'll need these for terraform.tfvars

## Step 2: Configure Terraform Variables

### 2.1 Update terraform.tfvars

```bash
cd terraform
cp terraform.tfvars.example terraform.tfvars
```

Edit `terraform.tfvars` and add your Google credentials:

```hcl
# Your van's identifier
thing_name = "storyteller-van-01"

# AWS Configuration
aws_region  = "us-east-1"
aws_profile = "skadi"  # Your AWS CLI profile

# Google OAuth Configuration
google_client_id     = "123456789-abcdefg.apps.googleusercontent.com"
google_client_secret = "GOCSPX-your_client_secret_here"

# Optional: Restrict API access by IP
allowed_ip_ranges = []
```

**⚠️ IMPORTANT:** Never commit `terraform.tfvars` to git! It contains secrets.

## Step 3: Deploy Cognito Infrastructure

### 3.1 Terraform Plan

```bash
cd terraform
terraform plan
```

You should see new resources:
- `aws_cognito_user_pool.van_users`
- `aws_cognito_user_pool_domain.van_auth`
- `aws_cognito_identity_provider.google`
- `aws_cognito_user_pool_client.van_dashboard`
- `aws_apigatewayv2_authorizer.cognito`
- Updated: `aws_apigatewayv2_route.*` (with authorization)
- Updated: `aws_s3_object.dashboard` (with Cognito config)
- New: `aws_s3_object.callback` (OAuth callback handler)

### 3.2 Apply Changes

```bash
terraform apply
```

### 3.3 Note the Outputs

After deployment, save these outputs:

```bash
terraform output cognito_hosted_ui_url
# https://storyteller-van-01-auth.auth.us-east-1.amazoncognito.com

terraform output cognito_user_pool_id
# us-east-1_XXXXXXXXX

terraform output cognito_login_url
# Full login URL for your dashboard
```

## Step 4: Create Your First User

### 4.1 Access Cognito Console

1. Go to [AWS Cognito Console](https://console.aws.amazon.com/cognito/)
2. Click on your User Pool (e.g., `storyteller-van-01-users`)

### 4.2 Create User

1. Click **Users** tab
2. Click **Create user**
3. Configure user:
   - **Email:** Your Google account email (must match!)
   - **Mark email as verified:** ✅ Checked
   - **Temporary password:** Create one (user won't use it, but required)
   - **Send an email invitation:** Optional (not needed for Google login)
4. Click **Create user**

### 4.3 Link Google Identity

When the user logs in with Google for the first time, Cognito will automatically link their Google account to the Cognito user profile based on the email address match.

## Step 5: Test Authentication

### 5.1 Access Dashboard

1. Go to your CloudFront URL:
   ```bash
   terraform output cloudfront_domain
   # https://d1234567890.cloudfront.net
   ```

2. You should be automatically redirected to Google login

### 5.2 Login with Google

1. Click **Continue with Google** (or similar)
2. Select your Google account
3. Authorize the app if prompted
4. You should be redirected back to the dashboard

### 5.3 Verify API Access

Once logged in:
- Dashboard should load telemetry data
- Check browser console (F12) for any errors
- Verify Authorization header is sent: DevTools → Network → Select request → Headers

Expected Authorization header:
```
Authorization: Bearer eyJraWQiOiJ...very_long_JWT_token...
```

## Step 6: Invite Additional Users

To give someone else access:

### 6.1 Create User in Cognito

1. AWS Console → Cognito → Your User Pool → Users → Create user
2. Enter their **Google account email**
3. Mark email as verified
4. Create temporary password (won't be used)

### 6.2 Share Dashboard URL

Send them:
- CloudFront URL
- Instructions to log in with Google
- Confirm they use the same Google account email you added

## Troubleshooting

### Error: "User does not exist"

**Solution:** Create user in Cognito console with exact Google account email

### Error: "Redirect URI mismatch"

**Solution:** 
1. Check Google OAuth settings → Authorized redirect URIs
2. Verify it matches: `https://<thing-name>-auth.auth.<region>.amazoncognito.com/oauth2/idpresponse`
3. Get actual Cognito domain from: `terraform output cognito_hosted_ui_url`

### Error: "Invalid credentials" or "Client authentication failed"

**Solution:**
1. Verify Google Client ID and Secret in terraform.tfvars
2. Run `terraform apply` to update Cognito identity provider
3. Check Google Cloud Console → Credentials → Your OAuth client is active

### Dashboard shows "401 Unauthorized"

**Causes:**
- Token expired (auto-redirects to login after 60 min)
- API Gateway authorizer misconfigured
- JWT token malformed

**Solution:**
1. Logout and login again
2. Check browser console for specific error
3. Verify `terraform output cognito_user_pool_id` matches authorizer config

### Can't access dashboard from mobile/Starlink

**Solution:**
1. Check `allowed_ip_ranges` in terraform.tfvars
2. Set to `[]` to allow all IPs (less secure but more flexible)
3. Or add your mobile carrier/Starlink IP ranges

## Security Best Practices

1. **Invite-only:** Never set `allow_admin_create_user_only = false`
2. **Token expiry:** Default 60 minutes - adjust in cognito.tf if needed
3. **HTTPS only:** Dashboard and API are HTTPS-only (enforced by CloudFront/API Gateway)
4. **Secrets:** Keep terraform.tfvars and Google Client Secret private
5. **IP restrictions:** Consider setting `allowed_ip_ranges` for known locations
6. **User cleanup:** Remove users in Cognito when they no longer need access

## Architecture

```
[User Browser]
    ↓ (1) Access dashboard
[CloudFront] → S3 (index.html)
    ↓ (2) No auth tokens → redirect
[Cognito Hosted UI]
    ↓ (3) User selects Google
[Google OAuth]
    ↓ (4) User authorizes
[Cognito Hosted UI]
    ↓ (5) Authorization code
[callback.html]
    ↓ (6) Exchange code for JWT tokens
[API Gateway]
    ↓ (7) API calls with Authorization: Bearer <token>
[Lambda Functions] → DynamoDB
```

## API Protection

All API routes now require authentication:
- `GET /telemetry` - Requires valid JWT
- `GET /telemetry/latest` - Requires valid JWT
- `POST /command` - Requires valid JWT

## Logout

To logout manually:
1. Open browser console (F12)
2. Run: `logout()`
3. Or close browser and clear sessionStorage

## Next Steps

- Monitor Cognito users in AWS Console
- Set up CloudWatch alarms for failed logins
- Consider adding MFA (multi-factor authentication) for production
- Review Cognito pricing (first 50,000 MAUs are free)

## Resources

- [AWS Cognito Documentation](https://docs.aws.amazon.com/cognito/)
- [Google OAuth 2.0 Documentation](https://developers.google.com/identity/protocols/oauth2)
- [JWT.io Debugger](https://jwt.io/) - Decode and inspect JWT tokens
