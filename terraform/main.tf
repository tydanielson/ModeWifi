terraform {
  required_version = ">= 1.0"
  
  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 5.0"
    }
  }
}

provider "aws" {
  region  = var.aws_region
  profile = var.aws_profile
  
  # For AWS SSO, ensure you've run: aws sso login --profile <name>
  # Terraform will use the SSO cached credentials automatically
}

# Get current AWS account ID automatically
data "aws_caller_identity" "current" {}

# Get AWS region automatically
data "aws_region" "current" {}

# IoT Thing for the van
resource "aws_iot_thing" "van" {
  name = var.thing_name
}

# IoT Policy - allows publish/subscribe on van topics
# SECURITY: Restricts device to specific client ID and topics only
resource "aws_iot_policy" "van_policy" {
  name = "${var.thing_name}-policy"

  policy = jsonencode({
    Version = "2012-10-17"
    Statement = concat(
      [
        # SECURITY: Only allow connection with exact client ID matching thing name
        {
          Effect = "Allow"
          Action = [
            "iot:Connect"
          ]
          Resource = "arn:aws:iot:${var.aws_region}:${data.aws_caller_identity.current.account_id}:client/${var.thing_name}"
          Condition = {
            StringEquals = {
              "iot:Connection.Thing.ThingName" = var.thing_name
            }
          }
        },
        # SECURITY: Only allow publishing to device-specific topics
        {
          Effect = "Allow"
          Action = [
            "iot:Publish"
          ]
          Resource = [
            "arn:aws:iot:${var.aws_region}:${data.aws_caller_identity.current.account_id}:topic/van/${var.thing_name}/telemetry",
            "arn:aws:iot:${var.aws_region}:${data.aws_caller_identity.current.account_id}:topic/van/${var.thing_name}/status",
            "arn:aws:iot:${var.aws_region}:${data.aws_caller_identity.current.account_id}:topic/van/${var.thing_name}/alerts"
          ]
        },
        # SECURITY: Only allow subscribing to device-specific command topics
        {
          Effect = "Allow"
          Action = [
            "iot:Subscribe"
          ]
          Resource = [
            "arn:aws:iot:${var.aws_region}:${data.aws_caller_identity.current.account_id}:topicfilter/van/${var.thing_name}/commands"
          ]
        },
        # SECURITY: Only allow receiving messages from device-specific topics
        {
          Effect = "Allow"
          Action = [
            "iot:Receive"
          ]
          Resource = [
            "arn:aws:iot:${var.aws_region}:${data.aws_caller_identity.current.account_id}:topic/van/${var.thing_name}/commands"
          ]
        }
      ],
      # OPTIONAL IP RESTRICTION: Add IP whitelist if specified
      length(var.allowed_ip_ranges) > 0 ? [
        {
          Effect = "Deny"
          Action = [
            "iot:Connect",
            "iot:Publish",
            "iot:Subscribe",
            "iot:Receive"
          ]
          Resource = "*"
          Condition = {
            NotIpAddress = {
              "aws:SourceIp" = var.allowed_ip_ranges
            }
          }
        }
      ] : []
    )
  })
}

# Get IoT endpoint
data "aws_iot_endpoint" "endpoint" {
  endpoint_type = "iot:Data-ATS"
}

# Note: We use data sources to automatically detect:
# - AWS Account ID: data.aws_caller_identity.current.account_id
# - AWS Region: data.aws_region.current.name
# This means users don't need to provide their account ID!

# Certificate and key (will be generated locally via script)
# Note: Terraform can't generate IoT certificates directly in a secure way
# We'll use a local-exec provisioner to generate them

resource "null_resource" "generate_certificates" {
  triggers = {
    thing_name = aws_iot_thing.van.name
  }

  provisioner "local-exec" {
    command = <<-EOT
      # Create certificates directory
      mkdir -p ${path.module}/../certificates
      
      # Generate certificate and keys
      aws iot create-keys-and-certificate \
        --set-as-active \
        --certificate-pem-outfile ${path.module}/../certificates/certificate.pem.crt \
        --public-key-outfile ${path.module}/../certificates/public.pem.key \
        --private-key-outfile ${path.module}/../certificates/private.pem.key \
        --region ${var.aws_region} \
        > ${path.module}/../certificates/certificate-info.json
      
      # Download root CA
      curl -o ${path.module}/../certificates/AmazonRootCA1.pem \
        https://www.amazontrust.com/repository/AmazonRootCA1.pem
      
      echo "Certificates generated in ../certificates/"
    EOT
  }
}

# Attach policy to certificate
# This requires the certificate ARN from the generated certificate-info.json
# We'll handle this in a separate step or via AWS CLI

# DynamoDB table for storing telemetry
# DESIGN: Single-table design - thing_name as partition key, timestamp as sort key
# Supports multiple devices with efficient time-series queries per device
resource "aws_dynamodb_table" "van_telemetry" {
  name           = "${var.thing_name}-telemetry"
  billing_mode   = "PAY_PER_REQUEST"  # Serverless - only pay for what you use
  hash_key       = "thing_name"        # Partition key - device identifier
  range_key      = "timestamp"         # Sort key - Unix timestamp (milliseconds)
  
  # SECURITY: Enable encryption at rest using AWS managed keys
  server_side_encryption {
    enabled = true
  }
  
  # SECURITY: Enable point-in-time recovery for backup/restore
  point_in_time_recovery {
    enabled = true
  }

  attribute {
    name = "thing_name"
    type = "S"
  }

  attribute {
    name = "timestamp"
    type = "N"
  }
  
  attribute {
    name = "message_type"
    type = "S"
  }
  
  attribute {
    name = "server_timestamp"
    type = "N"
  }
  
  # GSI for querying by message type across all devices
  # Example: Get all "alert" messages from last 24 hours
  global_secondary_index {
    name            = "MessageTypeIndex"
    hash_key        = "message_type"
    range_key       = "timestamp"
    projection_type = "ALL"
  }
  
  # GSI for efficiently querying latest telemetry by server timestamp
  # Allows fast lookup of most recent data without scanning entire table
  global_secondary_index {
    name            = "ThingServerTimestampIndex"
    hash_key        = "thing_name"
    range_key       = "server_timestamp"
    projection_type = "ALL"
  }

  # Auto-delete old data after 30 days to control costs
  ttl {
    attribute_name = "ttl"
    enabled        = true
  }

  tags = {
    Project = "VanControl"
    Thing   = var.thing_name
  }
}

# CloudWatch Log Group for IoT errors
resource "aws_cloudwatch_log_group" "iot_errors" {
  name              = "/aws/iot/${var.thing_name}/errors"
  retention_in_days = 7  # Keep logs for 7 days

  tags = {
    Project = "VanControl"
    Thing   = var.thing_name
  }
}

# IAM role for IoT to write logs to CloudWatch
resource "aws_iam_role" "iot_cloudwatch_role" {
  name = "${var.thing_name}-iot-cloudwatch-role"

  assume_role_policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Effect = "Allow"
        Principal = {
          Service = "iot.amazonaws.com"
        }
        Action = "sts:AssumeRole"
      }
    ]
  })
}

resource "aws_iam_role_policy" "iot_cloudwatch_policy" {
  name = "${var.thing_name}-iot-cloudwatch-policy"
  role = aws_iam_role.iot_cloudwatch_role.id

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
        Resource = "${aws_cloudwatch_log_group.iot_errors.arn}:*"
      }
    ]
  })
}

# IAM role for IoT to write to DynamoDB
resource "aws_iam_role" "iot_dynamodb_role" {
  name = "${var.thing_name}-iot-dynamodb-role"

  assume_role_policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Effect = "Allow"
        Principal = {
          Service = "iot.amazonaws.com"
        }
        Action = "sts:AssumeRole"
      }
    ]
  })
}

resource "aws_iam_role_policy" "iot_dynamodb_policy" {
  name = "${var.thing_name}-iot-dynamodb-policy"
  role = aws_iam_role.iot_dynamodb_role.id

  policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Effect = "Allow"
        Action = [
          "dynamodb:PutItem"
        ]
        Resource = aws_dynamodb_table.van_telemetry.arn
      }
    ]
  })
}

# IoT Rule to store telemetry in DynamoDB
# SECURITY: CloudWatch logging enabled for monitoring
resource "aws_iot_topic_rule" "store_telemetry" {
  name        = "${replace(var.thing_name, "-", "_")}_store_telemetry"
  enabled     = true
  sql         = "SELECT *, timestamp() as server_timestamp FROM 'van/${var.thing_name}/telemetry'"
  sql_version = "2016-03-23"
  
  # SECURITY: Enable error logging to CloudWatch
  error_action {
    cloudwatch_logs {
      log_group_name = aws_cloudwatch_log_group.iot_errors.name
      role_arn       = aws_iam_role.iot_cloudwatch_role.arn
    }
  }

  dynamodbv2 {
    role_arn = aws_iam_role.iot_dynamodb_role.arn
    put_item {
      table_name = aws_dynamodb_table.van_telemetry.name
    }
  }
}
