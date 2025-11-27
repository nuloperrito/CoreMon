# Set the end date 50 years from now
$endDate = (Get-Date).AddYears(50); 

# Create an empty SecureString for the password (no password in this example, but kept for PFX export consistency)
$securePass = New-Object System.Security.SecureString;

# 1. GENERATE THE CERTIFICATE
# *** KEY CHANGE: Added -Type CodeSigning ***
New-SelfSignedCertificate `
    -Subject "CN=Pablo" `
    -KeyAlgorithm RSA `
    -KeyLength 2048 `
    -HashAlgorithm SHA256 `
    -CertStoreLocation Cert:\CurrentUser\My `
    -NotAfter $endDate `
    -FriendlyName "Pablo Self-Signed 50Y Code Signing" `
    -Type CodeSigning | 
# 2. EXPORT THE CERTIFICATE
Export-PfxCertificate `
    -FilePath "CoreMon_TemporaryKey.pfx" `
    -Password $securePass