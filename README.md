boot.cfg
- specified the defaul ini file to load at reset of the Floppy80

ini files
- specifies the disk images and options after reset

ini options
- Drive0 - specified the image to load for drive :0
- Drive1 - specified the image to load for drive :1
- Drive2 - specified the image to load for drive :2
- HD0 - specifies the image to load for tyhe first hard drive
- HD1 - specifies the image to load for tyhe second hard drive
- Doubler - 1 = doubler is enabled; 0 = doubler is disabled;

FDC
- Is a utility to minteract with the Floppy80 from withing the
  TRS-80 Model I operating environment.

- Usage

  FDC OPT PARM1:drive

  Where:
  - OPT is one of the following
    - STA - returns the status of the Floppy80.
    - DIR - displays a list of file in the root folder of the SD-Card.
    - INI - switches between tyhe differnt ini file on the SD-Card.
    - DMK - allows the mounting of DMK disk images in the root folder
            of the SD-Card for a specified drive (0, 1 or 2).
    - FOR - copys a DMK disk image from the FOR folder of the SD-Card
            to one of the mounted disk images (0, 1 or 2).
    - IMP - imports a file from the root folder of the SD-Card
            into one of the mounted disk images (0, 1 or 2).

- PARM1:drive is optional.  However when specified after the
  INI, DMK or IMP options will bypass the file selection process.

- FDC INI filename.ext
  - write the specified filename and extension to the boot.cfg file.

- FDC DMK filename.ext n
  - mounts DMK image specified by filename.ext onto the drive specified by n.

- FDC IMP filename.ext:n
  - copies filename.ext from the root of the SD-Card to to the disk image n.
