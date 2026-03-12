# VULNERABILITY 1: Hardcoded Secrets
# Never bake credentials into your code. Use environment variables or a Secrets Manager.
provider "aws" {
  region     = "us-east-1"
  access_key = "AKIAEXAMPLE123456789" 
  secret_key = "wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY"
}

# VULNERABILITY 2: Publicly Accessible S3 Bucket
# This bucket has no access control, making sensitive data available to the entire internet.
resource "aws_s3_bucket" "insecure_bucket" {
  bucket = "my-very-secret-data-12345"
}

resource "aws_s3_bucket_public_access_block" "bad_practice" {
  bucket = aws_s3_bucket.insecure_bucket.id

  block_public_acls       = false
  block_public_policy     = false
  ignore_public_acls      = false
  restrict_public_buckets = false
}

# VULNERABILITY 3: Overly Permissive Security Group
# This allows anyone on the internet (0.0.0.0/0) to SSH into your instance.
resource "aws_security_group" "allow_ssh_all" {
  name        = "allow_ssh"
  description = "Allow SSH from anywhere"

  ingress {
    from_port   = 22
    to_port     = 22
    protocol    = "tcp"
    cidr_blocks = ["0.0.0.0/0"] # The "everyone" flag
  }
}
