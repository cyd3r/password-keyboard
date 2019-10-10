# Arduino Password Keyboard

Plug in this device and select a password to type. The device acts like a keyboard and will type the selected passphrase.

All passwords are stored in an AES encrypted storage.

## Parts needed

- Arduino Micro
- OLED display
- 3 push buttons

## Usage

### Get a password

Use the next and previous buttons to navigate through the list of available accounts. Press the submit button and you will be prompted to enter the master password. Use all available buttons on the device to enter the 8-digit password.

On success, the account password will be entered on the connected PC. Keep in mind that the device acts like a normal keyboard.

### Add a new password

The device has no keyboard so adding a new password must involve a computer. Use the python utility to add a password:

    python3 manage.py add

Then, input the account name and the password. The device will ask you to enter the password.

Accounts that exist, will be overwritten. Account names are case sensitive.

### Remove a password

To remove a password, launch the python utility:

    python3 manage.py rm

Enter the account name you want to remove and press enter. There is no authentication required on the device for now.

### Set the master password

Launch the python utility:

    python3 manage.py reset

This will remove all your stored passwords and will reset the master password.

## Storage

Passwords are stored on an (external) EEPROM. For each account, 16 bytes are allocated for the account name and 32 bytes are required for the encrypted password (48 bytes in total).

With an EEPROM with 8kB, 170 accounts are possible.

## Encryption

The password files are AES128 encrypted. The AES key is generated from a sequence of pressed buttons with length 8.

## TODOs

- solve keyboard layouts, currently US layout :/
- add wrong password cooldown
- add better account navigation
- use external EEPROM
- add more buttons for master password input
- add schematics
- refactor
