If (!(Test-Path "$PSScriptRoot\diff-pdf.exe")) {
	echo ""; echo "diff-pdf.exe not found in '$PSScriptRoot'. Exiting."; echo ""; Pause; Exit
}

If (($args).count -eq 0) {
  # Requires InvocationName instead of MyCommand due to being called as an argument of powershell.exe via a shortcut
	$ScriptName = (Get-Item $MyInvocation.InvocationName).BaseName
	If (Test-Path "$env:appdata\Microsoft\Windows\SendTo\$ScriptName.lnk") {
		Remove-Item "$env:appdata\Microsoft\Windows\SendTo\$ScriptName.lnk"
	}
	$s=(New-Object -COM WScript.Shell).CreateShortcut("$env:appdata\Microsoft\Windows\SendTo\$ScriptName.lnk")
	$s.TargetPath="powershell.exe"
	$s.Arguments="-File `"$PSScriptRoot\$ScriptName.ps1`""
	$s.Save()
	echo ""; echo "SendTo shortcut created"; echo ""; Timeout /T 3; Exit
}

If (($args).count -ne 2) {
	echo ""
	If (($args).count -lt 2) {
		echo "Not enough files selected for comparison. Select TWO PDFs and try again."
	} else {
		echo "Too many files selected for comparison. Select TWO PDFs and try again."
	}
	echo ""; Pause; Exit
}

ForEach ($item in $args) {
	If ([IO.Path]::GetExtension($item) -ne ".pdf") {
		echo ""; echo "One or both selected files is NOT a PDF. Select two PDFs and try again."; echo ""; Pause; Exit
	}
}

# Uncomment this line and comment next line to view differences in diff-pdf's viewer (does NOT create additional file)
& "$PSScriptRoot\diff-pdf.exe" --view $args[0] $args[1]

# Uncomment this line and comment previous line to create new PDF file showing differences between files. File will be named "diff_{date_time}" where {date_time} is in yyyy-MM-dd_HH.mm.ss format
#$date_time = Get-Date -Format "yyyy-MM-dd_HH.mm.ss"; & "$PSScriptRoot\diff-pdf.exe" --output-diff=diff_$date_time.pdf $args[0] $args[1]; $PDF_Dir=Split-Path -Path $args[0] -Parent; Do {Start-Sleep -Seconds 1} Until (Test-Path "$PDF_Dir\diff_$date_time.pdf"); & "$PDF_Dir\diff_$date_time.pdf"
