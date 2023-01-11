Run A_Args[1]

WinWait("WinPcap 4.1.3 Setup", , 30)

WinExist
{
	BlockInput true
	WinActivate
	Sleep 250
	Send "{Enter}"
	Sleep 250
	Send "{Enter}"
	Sleep 250
	Send "{Enter}"
	Sleep 250
	Send "{Enter}"
	WinWait("WinPcap 4.1.3 Setup", "Click Finish", 30)
	Send "{Enter}"
	BlockInput false
}


ExitApp