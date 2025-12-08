# S3 bucket for hosting the web application
resource "aws_s3_bucket" "webapp" {
  bucket = "${var.thing_name}-webapp"
  
  tags = {
    Name        = "${var.thing_name}-webapp"
    Environment = "production"
  }
}

resource "aws_s3_bucket_public_access_block" "webapp" {
  bucket = aws_s3_bucket.webapp.id

  block_public_acls       = true
  block_public_policy     = true
  ignore_public_acls      = true
  restrict_public_buckets = true
}

resource "aws_s3_bucket_versioning" "webapp" {
  bucket = aws_s3_bucket.webapp.id
  
  versioning_configuration {
    status = "Enabled"
  }
}

# CloudFront Origin Access Control
resource "aws_cloudfront_origin_access_control" "webapp" {
  name                              = "${var.thing_name}-webapp-oac"
  description                       = "OAC for ${var.thing_name} webapp"
  origin_access_control_origin_type = "s3"
  signing_behavior                  = "always"
  signing_protocol                  = "sigv4"
}

# CloudFront distribution
resource "aws_cloudfront_distribution" "webapp" {
  enabled             = true
  is_ipv6_enabled     = true
  comment             = "${var.thing_name} webapp distribution"
  default_root_object = "index.html"
  price_class         = "PriceClass_100" # US, Canada, Europe

  origin {
    domain_name              = aws_s3_bucket.webapp.bucket_regional_domain_name
    origin_id                = "S3-${aws_s3_bucket.webapp.id}"
    origin_access_control_id = aws_cloudfront_origin_access_control.webapp.id
  }

  default_cache_behavior {
    allowed_methods  = ["GET", "HEAD", "OPTIONS"]
    cached_methods   = ["GET", "HEAD"]
    target_origin_id = "S3-${aws_s3_bucket.webapp.id}"

    forwarded_values {
      query_string = false
      cookies {
        forward = "none"
      }
    }

    viewer_protocol_policy = "redirect-to-https"
    min_ttl                = 0
    default_ttl            = 3600
    max_ttl                = 86400
    compress               = true
  }

  # Custom error responses for SPA routing
  custom_error_response {
    error_code         = 404
    response_code      = 200
    response_page_path = "/index.html"
  }

  custom_error_response {
    error_code         = 403
    response_code      = 200
    response_page_path = "/index.html"
  }

  restrictions {
    geo_restriction {
      restriction_type = "none"
    }
  }

  viewer_certificate {
    cloudfront_default_certificate = true
  }

  tags = {
    Name        = "${var.thing_name}-webapp"
    Environment = "production"
  }
}

# S3 bucket policy to allow CloudFront access
resource "aws_s3_bucket_policy" "webapp" {
  bucket = aws_s3_bucket.webapp.id

  policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Sid    = "AllowCloudFrontServicePrincipal"
        Effect = "Allow"
        Principal = {
          Service = "cloudfront.amazonaws.com"
        }
        Action   = "s3:GetObject"
        Resource = "${aws_s3_bucket.webapp.arn}/*"
        Condition = {
          StringEquals = {
            "AWS:SourceArn" = aws_cloudfront_distribution.webapp.arn
          }
        }
      }
    ]
  })
}

# Dashboard HTML with API URL substitution
resource "aws_s3_object" "dashboard" {
  bucket       = aws_s3_bucket.webapp.id
  key          = "index.html"
  content      = replace(
    file("${path.module}/../dashboard/index.html"),
    "$${API_GATEWAY_URL}",
    aws_apigatewayv2_stage.webapp.invoke_url
  )
  content_type = "text/html"
  etag         = md5(replace(
    file("${path.module}/../dashboard/index.html"),
    "$${API_GATEWAY_URL}",
    aws_apigatewayv2_stage.webapp.invoke_url
  ))

  tags = {
    Name        = "${var.thing_name}-dashboard"
    Environment = "production"
  }
}

# API Gateway HTTP API (v2)
resource "aws_apigatewayv2_api" "webapp" {
  name          = "${var.thing_name}-api"
  protocol_type = "HTTP"
  
  cors_configuration {
    allow_origins = [
      "https://${aws_cloudfront_distribution.webapp.domain_name}",
      "http://localhost:3000" # For local development
    ]
    allow_methods = ["GET", "POST", "PUT", "DELETE", "OPTIONS"]
    allow_headers = ["*"]
    max_age       = 300
  }

  tags = {
    Name        = "${var.thing_name}-api"
    Environment = "production"
  }
}

# API Gateway stage
resource "aws_apigatewayv2_stage" "webapp" {
  api_id      = aws_apigatewayv2_api.webapp.id
  name        = "prod"
  auto_deploy = true

  access_log_settings {
    destination_arn = aws_cloudwatch_log_group.api_gateway.arn
    format = jsonencode({
      requestId      = "$context.requestId"
      ip             = "$context.identity.sourceIp"
      requestTime    = "$context.requestTime"
      httpMethod     = "$context.httpMethod"
      routeKey       = "$context.routeKey"
      status         = "$context.status"
      protocol       = "$context.protocol"
      responseLength = "$context.responseLength"
    })
  }

  tags = {
    Name        = "${var.thing_name}-api-stage"
    Environment = "production"
  }
}

# CloudWatch Log Group for API Gateway
resource "aws_cloudwatch_log_group" "api_gateway" {
  name              = "/aws/apigateway/${var.thing_name}"
  retention_in_days = 7

  tags = {
    Name        = "${var.thing_name}-api-logs"
    Environment = "production"
  }
}

# IAM role for Lambda functions
resource "aws_iam_role" "lambda" {
  name = "${var.thing_name}-lambda-role"

  assume_role_policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Action = "sts:AssumeRole"
        Effect = "Allow"
        Principal = {
          Service = "lambda.amazonaws.com"
        }
      }
    ]
  })

  tags = {
    Name        = "${var.thing_name}-lambda-role"
    Environment = "production"
  }
}

# IAM policy for Lambda to access DynamoDB and CloudWatch
resource "aws_iam_role_policy" "lambda" {
  name = "${var.thing_name}-lambda-policy"
  role = aws_iam_role.lambda.id

  policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Effect = "Allow"
        Action = [
          "logs:CreateLogGroup",
          "logs:CreateLogStream",
          "logs:PutLogEvents"
        ]
        Resource = "arn:aws:logs:*:*:*"
      },
      {
        Effect = "Allow"
        Action = [
          "dynamodb:GetItem",
          "dynamodb:Query",
          "dynamodb:Scan"
        ]
        Resource = [
          aws_dynamodb_table.van_telemetry.arn,
          "${aws_dynamodb_table.van_telemetry.arn}/index/*"
        ]
      },
      {
        Effect = "Allow"
        Action = [
          "iot:Publish"
        ]
        Resource = "arn:aws:iot:${data.aws_region.current.name}:${data.aws_caller_identity.current.account_id}:topic/van/${var.thing_name}/commands"
      }
    ]
  })
}

# Package Lambda functions automatically
data "archive_file" "get_telemetry" {
  type        = "zip"
  source_dir  = "${path.module}/../lambda/get-telemetry"
  output_path = "${path.module}/../lambda/get-telemetry.zip"
}

data "archive_file" "send_command" {
  type        = "zip"
  source_dir  = "${path.module}/../lambda/send-command"
  output_path = "${path.module}/../lambda/send-command.zip"
}

# Lambda function to get telemetry data
resource "aws_lambda_function" "get_telemetry" {
  filename         = data.archive_file.get_telemetry.output_path
  function_name    = "${var.thing_name}-get-telemetry"
  role            = aws_iam_role.lambda.arn
  handler         = "index.handler"
  source_code_hash = data.archive_file.get_telemetry.output_base64sha256
  runtime         = "nodejs20.x"
  timeout         = 10

  environment {
    variables = {
      TABLE_NAME = aws_dynamodb_table.van_telemetry.name
      THING_NAME = var.thing_name
    }
  }

  tags = {
    Name        = "${var.thing_name}-get-telemetry"
    Environment = "production"
  }
}

# Lambda function to send commands
resource "aws_lambda_function" "send_command" {
  filename         = data.archive_file.send_command.output_path
  function_name    = "${var.thing_name}-send-command"
  role            = aws_iam_role.lambda.arn
  handler         = "index.handler"
  source_code_hash = data.archive_file.send_command.output_base64sha256
  runtime         = "nodejs20.x"
  timeout         = 10

  environment {
    variables = {
      THING_NAME = var.thing_name
      IOT_ENDPOINT = data.aws_iot_endpoint.endpoint.endpoint_address
    }
  }

  tags = {
    Name        = "${var.thing_name}-send-command"
    Environment = "production"
  }
}

# CloudWatch Log Groups for Lambda
resource "aws_cloudwatch_log_group" "get_telemetry" {
  name              = "/aws/lambda/${aws_lambda_function.get_telemetry.function_name}"
  retention_in_days = 7

  tags = {
    Name        = "${var.thing_name}-get-telemetry-logs"
    Environment = "production"
  }
}

resource "aws_cloudwatch_log_group" "send_command" {
  name              = "/aws/lambda/${aws_lambda_function.send_command.function_name}"
  retention_in_days = 7

  tags = {
    Name        = "${var.thing_name}-send-command-logs"
    Environment = "production"
  }
}

# API Gateway Lambda integrations
resource "aws_apigatewayv2_integration" "get_telemetry" {
  api_id           = aws_apigatewayv2_api.webapp.id
  integration_type = "AWS_PROXY"
  integration_uri  = aws_lambda_function.get_telemetry.invoke_arn
  
  payload_format_version = "2.0"
}

resource "aws_apigatewayv2_integration" "send_command" {
  api_id           = aws_apigatewayv2_api.webapp.id
  integration_type = "AWS_PROXY"
  integration_uri  = aws_lambda_function.send_command.invoke_arn
  
  payload_format_version = "2.0"
}

# API Gateway routes
resource "aws_apigatewayv2_route" "get_telemetry" {
  api_id    = aws_apigatewayv2_api.webapp.id
  route_key = "GET /telemetry"
  target    = "integrations/${aws_apigatewayv2_integration.get_telemetry.id}"
}

resource "aws_apigatewayv2_route" "get_latest_telemetry" {
  api_id    = aws_apigatewayv2_api.webapp.id
  route_key = "GET /telemetry/latest"
  target    = "integrations/${aws_apigatewayv2_integration.get_telemetry.id}"
}

resource "aws_apigatewayv2_route" "send_command" {
  api_id    = aws_apigatewayv2_api.webapp.id
  route_key = "POST /command"
  target    = "integrations/${aws_apigatewayv2_integration.send_command.id}"
}

# Lambda permissions for API Gateway
resource "aws_lambda_permission" "get_telemetry" {
  statement_id  = "AllowAPIGatewayInvoke"
  action        = "lambda:InvokeFunction"
  function_name = aws_lambda_function.get_telemetry.function_name
  principal     = "apigateway.amazonaws.com"
  source_arn    = "${aws_apigatewayv2_api.webapp.execution_arn}/*/*"
}

resource "aws_lambda_permission" "send_command" {
  statement_id  = "AllowAPIGatewayInvoke"
  action        = "lambda:InvokeFunction"
  function_name = aws_lambda_function.send_command.function_name
  principal     = "apigateway.amazonaws.com"
  source_arn    = "${aws_apigatewayv2_api.webapp.execution_arn}/*/*"
}
