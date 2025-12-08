output "iot_endpoint" {
  description = "AWS IoT endpoint for MQTT connection"
  value       = data.aws_iot_endpoint.endpoint.endpoint_address
}

output "thing_name" {
  description = "IoT Thing name"
  value       = aws_iot_thing.van.name
}

output "thing_arn" {
  description = "IoT Thing ARN"
  value       = aws_iot_thing.van.arn
}

output "dynamodb_table" {
  description = "DynamoDB table for telemetry storage"
  value       = aws_dynamodb_table.van_telemetry.name
}

output "certificate_instructions" {
  description = "Instructions for certificate setup"
  value       = <<-EOT
    Certificates have been generated in ../certificates/
    
    Next steps:
    1. Get certificate ARN: cat ../certificates/certificate-info.json | jq -r '.certificateArn'
    2. Attach policy: aws iot attach-policy --policy-name ${aws_iot_policy.van_policy.name} --target <CERTIFICATE_ARN>
    3. Attach certificate to thing: aws iot attach-thing-principal --thing-name ${var.thing_name} --principal <CERTIFICATE_ARN>
    4. Copy certificates to ESP32 code in src/config.h
    
    IoT Endpoint: ${data.aws_iot_endpoint.endpoint.endpoint_address}
  EOT
}

# Cloud Dashboard Outputs
output "cloudfront_distribution_id" {
  description = "CloudFront distribution ID"
  value       = aws_cloudfront_distribution.webapp.id
}

output "cloudfront_domain" {
  description = "CloudFront distribution domain name (use this for CNAME if using custom domain)"
  value       = "https://${aws_cloudfront_distribution.webapp.domain_name}"
}

output "custom_domain_instructions" {
  description = "Instructions for setting up custom domain"
  value       = var.custom_domain != "" ? "Custom Domain Setup:\n1. Add this CNAME record in your danielson.io DNS:\n   Name: ${var.custom_domain}\n   Type: CNAME\n   Value: ${aws_cloudfront_distribution.webapp.domain_name}\n   TTL: 300\n\n2. Your dashboard will be available at: https://${var.custom_domain}\n3. Update Cognito callback URL to include custom domain (already configured in terraform)" : "No custom domain configured. Set custom_domain and acm_certificate_arn in terraform.tfvars to enable."
}

output "webapp_bucket" {
  description = "S3 bucket for web application"
  value       = aws_s3_bucket.webapp.id
}

output "api_gateway_url" {
  description = "API Gateway endpoint URL"
  value       = aws_apigatewayv2_stage.webapp.invoke_url
}

output "api_endpoints" {
  description = "API endpoints for the web application"
  value = {
    get_telemetry        = "${aws_apigatewayv2_stage.webapp.invoke_url}/telemetry"
    get_latest_telemetry = "${aws_apigatewayv2_stage.webapp.invoke_url}/telemetry/latest"
    send_command         = "${aws_apigatewayv2_stage.webapp.invoke_url}/command"
  }
}

output "deployment_instructions" {
  description = "Instructions for deploying the web application"
  value       = <<-EOT
    Web Application Deployment:
    
    1. CloudFront URL: https://${aws_cloudfront_distribution.webapp.domain_name}
    2. API Gateway URL: ${aws_apigatewayv2_stage.webapp.invoke_url}
    
    To deploy the frontend:
    - cd ../dashboard
    - npm install
    - npm run build
    - aws s3 sync build/ s3://${aws_s3_bucket.webapp.id}/ --delete
    - aws cloudfront create-invalidation --distribution-id ${aws_cloudfront_distribution.webapp.id} --paths "/*"
    
    API Endpoints:
    - GET  ${aws_apigatewayv2_stage.webapp.invoke_url}/telemetry
    - GET  ${aws_apigatewayv2_stage.webapp.invoke_url}/telemetry/latest
    - POST ${aws_apigatewayv2_stage.webapp.invoke_url}/command
  EOT
}

