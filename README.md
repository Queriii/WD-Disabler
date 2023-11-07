Old method for disabling Windows Defender by continuously unmapping (crashing) the usermode process, eventually, the kernel component stops trying to restart the process, effectively disabling Windows Defender.

Should be more viewed as a POC as there's no support provided for mapping an unsigned driver. 
