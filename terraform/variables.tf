variable "aws_region" {
  description = "AWS region for IoT resources"
  type        = string
  default     = "us-east-1"
}

variable "aws_profile" {
  description = "AWS CLI profile to use (e.g., 'default' or 'production')"
  type        = string
  default     = "default"
}

variable "thing_name" {
  description = "Name for the IoT Thing (your van)"
  type        = string
  default     = "storyteller-van-01"
}

variable "allowed_ip_ranges" {
  description = "CIDR blocks allowed to connect (empty = allow all)"
  type        = list(string)
  default     = []  # Empty = no IP restriction. Add your IPs for stricter security
  # Example: ["1.2.3.4/32", "5.6.7.0/24"]  # Your home/Starlink IPs
}
