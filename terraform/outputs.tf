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
