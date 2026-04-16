# Doc for SNESTOOL v1.2 (C) THE MCA / ELITE

Greetz fly out to the following groups:

Anthrox - Asgard - Cyberforce - Cream - Darkness - Edge - Legend -
Nightfall - Napalm - Oldskool - Sneakers - Sonic - Swat - Quasar

---

## Update on Snes-Tool v1.11 towards v1.2

Major bug fixed on conversion of GD3 files towards FIG/SWC format.
Seems that the info we had on this is incorrect. We stated in the previous
Snes-Tool that there was no need for converting of GD3 files under the size
of 16 Mb. Now it seems there is.., So added now is converting of 4 Mb up to
16 Mb from GD3 to FIG/SWC and visa versa. Thanks to Caligula for pointing
this out.
SRAM disable is auto-added towards the Header after conversion.
If the game wont work after that, go to Repair Header to Enable the SRAM.
Also added is conversion of 48 Mb up to 64 Mb GD3 format towards FIG/SWC
since we dont have any specific info on this yet we aint sure if it is
correct, this conversion part is based on the most logical way that the
memory will be used by the Copier.
Did anybody actually get this 48 Mb (as-tales.lzh) file working ?

---

## Update on Snes-Tool v1.10 towards v1.11

A few improvements have been made towards the File Selecter.
You can press " S " to sort the files on Name or Extension.
This way its easier to pick out the Splitted or 1 parted files.
Also if the is a " . " behind the filesize. This indicates that
the game file has a special attribute on it ( hidden for example )

Also thanks to SubZero for pointing out that Sending of splitted games
like Mortal Combat III in 32 parts of 1 Mb didnt work. Would be highly
unlikely anybody would ever try do this, but now you can.

Its now also impossible to Split, already Splitted parts ( if you choose
the wrong part by accident ).

---

## Update on Snes-Tool v1.04 towards v1.10

Well this version has plenty more to offer then before, main thing that
was added was support for another Copier.
The SF Game Doctor 3, thanks to Dark Knight for pointing out that this
Copier has a COM port , i sure as hell didnt know that..
Also tnkx to Dark Knight for testing out: sending and converting files !

PAL/NTSC/SLOWROM will Auto-FIX now ALSO when the game is Split up in Parts.

File Selector has "spaces" added in between 'file name' and 'extension'
so now its much easier to to pick out the files you need.

Also there was a small bug in the amount of file entries in a Directory
it has been taken care of.

SPLIT option couldnt be stopped with ESC when you selected it by mistake.

Splitting files has been changed, you can now split every size you wish
from 1 Mb to 12 Mb. ( above this would be nonsense since splitting odd
sizes is only interesting for DISK use).

GD 3 files can be Joined and Splitted ( without converting ! )
Cart Info can be read from GD 3 files as well.

GD 3 to FIG ( swc/smc ) converts 20 Mb up to 32 Mb to HIGHROM or SMC
format. Files that have the size from 2 Mb to 16 Mb can be " converted "
with Add Header. Source files have to be in parts of 8 Mb.

FIG to GD 3 converts 20 MB up to 32 Mb FIG/SMC files to GD 3 files.
Source files can be splitted or in 1 part it doesnt matter.

### What didnt make it to this version yet?

Sending Highrom games towards the SF Game Doctor 3 seem to be a bit
of a weird problem, but it will be sorted in the near future.
Also some more sorting ( size/extension ) in the File Selector will
be added in the next future version.

---

## Update (unnumbered)

Some new improvments have been made, finally inclueded are
sorting of Directories and Files in Alphabetic order.

And Renaming of FILES or DIRECTORIES is added as well, this is
handy when creating IPS files!
Just press the " R " button when you are in the Selector!

Also fixed is the Hex Counter when sending Games bigger then 16 Mb
in one file, the counter was wrong.

When splitting a game ( for example 12 Mb ) would be split into
8 Mb and 4 Mb the header of the last file was wrong!
I never tested it on disk since my drive was fucked..
When you send to the Comport this isnt a problem at all..
Anyway its fixed now..

---

## Update on Snes-Tool v1.02 towards v1.03

A bug was found in the PAL & Slowrom fix routines, just found
when fixing Dracula X from Konami..
When Slowrom fixing the second file of this game, the SnesTool
would simply crash...
A Memory Boundery Fuckup for the experts..

---

## Update on Snes-Tool v1.01 towards v1.02

Thanks to Caligula for pointing out this small bug.
When splitting a game the header of the last file was still wrong.

Also we added A lot more companies to the Game Info area so the term
" FUCKED UP " wont pop up that much anymore..

---

## Update on Snes-Tool v1.0 towards v1.01

Thanks to Dan of Prestige for pointing out the bugs!
Good thing, since now they have been taken care of..

Following bugs have been fixed:

- When a game was Split, the first file was Hidden..
- IPS Creating fucked up on big files.
- IPS 2 ( cutting files ) Create and Use of a IPS 2 file work ok now.
- Header of the last file wasnt fixed properly, so it could ask for
  another file that wasnt there..

---

## History

After having a lot of experience with Snes programming on the
Atari ST & Falcon, I bought myself a 486dx2 PC, and tried to
rewrite some of my old Snes stuff.

There are a lot of snes utilities for the PC, but none of them
contains ALL important stuff, or is easy to use.
I hated the long typing before something happened, and installing
the most used functions under a shell, did not always work, different
directory's, etc..
So I had to make it Complete and Easy to use, with no typing at all!

Well, let's go on about explaining some functions...

## Functions

### Configure

This is important for adding headers, sending hi-rom games, and
fixing headers.
First the Printer port (1-4) is asked, if none set 1=default.
The function gets the port values from segment 40 (bios variables).

Then it asks ya for 5 different style of copiers, press Y if you
have the correct one, Double profighter works ok, Procom/Magicom also
And the Profighter Q+ 32 mbit is ok as well, the others are NOT tested
yet. So if Hi-rom sending does not work, try some other copiers
instead, there are 5 different types of Hirom Sending.

If you have made your choice, the fileselector gets active, then
choose the SNESTOOL and press return, it will then patch the
snestool.exe file to keep the values you entered.
You can skip the patching, but why re-enter it everytime huh?
Changing your settings as many times as you like is no problem
you can patch the Snestool.exe until you drop..

### Tab

Go to browsing mode, exit with Esc or `<TAB>` again.
Use this to switch between File-Selector and Menu.

### Split

Pick a snes file, then you can choose the split size for 4/8/12 mbit,
with the arrow keys, press return to start.
It will keep the 1st 8 letters name the extension to 1/2/3/etc..

### Join

Will only take `*.1` files then renames the file to `*.smc`.
Works on multiple files.

### Slowromfix

Searches & patches the selected file for some standard fixes.
Number of fixes will be displayed.
Almost fixes the same ones as killem31.
Killem is a little better however, but sometimes fixes a little
too much...

### Palfix

Search & patch standard protections for games which do not work on
PAL machines.

### NTSC Fix

Same but for games that do not work on NTSC machines.
Once fixed it must work on both machines.

### Use IPS

Apply the International Patch Standard on a file.
This type was invented by DAX and ME, we got a lot
of success with it, because we released the programs
on ATARI/AMIGA AND PC format at the same time.

Pick an IPS file first, the the game to be patched.
If IPS2 is detected, it will write a temporary file
called TEMP.SMC then renames it back to the original.
IPS2 files are ment to 'Cut' a file.
Kill them fucking advertisement Intro's...

### Create IPS

Compares 2 files, then write the difference to a file called
`2ndname.IPS`.

### Add Header

Test the size, and if ok writes a TEMP.SMC, renaming it afterwoods.
Works on multiple files.

### Repair Header

Auto detects multiple files, ask for S-ram on/off, Hirom yes/no,
with wildcard selected, also for mode 24 (other place of S-ram).
Then patches all files accordingly.

### Remove Intro

Tries to find and restore the original reset vector then cut's off
the intro (if added to the end).
Does NOT work on the latest Anthrox intro's, the ones who modify
a lot of 'blanking instructions'.
Does NOT work with the 'Hidden reset vector' stuff.

### Send & Run

Pick a file to send, make sure you have selected the correct
configuration, else strange things might happen.
It will detect multiple files and send the rest also.
After loading 1 or 2 blocks a file info menu will pop up,
containing some useful information about the game.
Some company's could be wrong, because they use the incorrect company-
value.
Esc cancels the sending.

### Slowfix On/Off

If ON each loaded block is scanned for fixes, if found it will
be patched in memory only, just before sending, at the end of
sending, the number of fixes will be displayed in the same box.

### File Info

Go to browsing mode by using `<TAB>` press return on a snes file to get
your information, exit with `<TAB>` again or Esc.

### Exit

Press ESC in SELECTION mode to quit.

### Choose Drive

Press A or C to Z to choose another Harddisk or CD-Rom.

### Delete

In the file selector you can press the DELETE key to kill a file..

## Fileselector Control Keys

Use the Cursor keys to Move Up and Down to select files
and Left and Right to go in and out of Directories.

## Hot-Keys

The HOT Keys are obvious since there Green and in Capitals.

---

For any suggestions, contact us on HiT & RuN BBS >> ELITE WHQ <<
Or find me on Internet somewhere..

---

This program is coded in MASM.

Dont pack this program or you wont be able to Patch your Copier
configuration!

No mouse support yet..maybe somebody wants to help out here?
This program can also be executed in Windows or OS/2.

Bye Sledge & MCA of ELITE
