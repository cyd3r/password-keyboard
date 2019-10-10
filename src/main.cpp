// pin layout
// 2 OLED-SDA
// 3 OLED-SCL
// 6 type password button
// 7 next account button
// 8 previous account button

// TODO: solve keyboard layouts
// TODO: add wrong password cooldown
// TODO: handle account limit

#include <Arduino.h>
#include <Keyboard.h>

// OLED
#include <Adafruit_SSD1306.h>

// passwords
#include <AES.h>
#include <EEPROM.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1 // my OLED screen has no reset pin
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

AES256 aes;

const int pinSubmit = 6;
int lastSubmitState = HIGH;

const int pinNextAccount = 7;
int lastNextAccountState = HIGH;
const int pinPrevAccount = 8;
int lastPrevAccountState = HIGH;

const int accountOffset = 1 + 16;
const int accountNameLength = 16;
int currentAccount = 0;
uint8_t numAccounts;

String inputBuffer;

// must be true: authLength % aes.keySize() == 0
const size_t authLength = 8;

bool btnIsPressed(int pin, int &last)
{
    int value = digitalRead(pin);
    if (value != last)
    {
        last = value;
        return value == LOW;
    }
    return false;
}

void displayPassword(int progress, uint8_t *buffer, char *info)
{
    display.clearDisplay();

    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println(info);

    display.setTextSize(2);
    display.setCursor(16, 24);
    for (int i = 0; i < progress; i++)
    {
        switch (buffer[i])
        {
        case 0x00u:
            display.print('0');
            break;
        case 0x01u:
            display.print('1');
            break;
        case 0x02u:
            display.print('2');
            break;
        default:
            display.print('?');
            break;
        }
    }
    for (size_t i = progress; i < authLength; i++)
    {
        display.print('.');
    }
    display.display();
}

void setKey(char *info)
{
    uint8_t buffer[authLength];
    size_t index = 0;
    displayPassword(0, buffer, info);
    while (index < authLength)
    {
        if (btnIsPressed(pinSubmit, lastSubmitState))
        {
            buffer[index] = 0x00;
            index++;
            displayPassword(index, buffer, info);
        }
        if (btnIsPressed(pinNextAccount, lastNextAccountState))
        {
            buffer[index] = 0x01;
            index++;
            displayPassword(index, buffer, info);
        }
        if (btnIsPressed(pinPrevAccount, lastPrevAccountState))
        {
            buffer[index] = 0x02;
            index++;
            displayPassword(index, buffer, info);
        }
    }

    uint8_t key[aes.keySize()];
    int repeat = aes.keySize() / authLength;
    for (size_t i = 0; i < authLength; i++)
    {
        for (int k = 0; k < repeat; k++)
        {
            key[i * repeat + k] = buffer[i];
        }
    }
    aes.setKey(key, aes.keySize());
}

bool validateKey()
{
    uint8_t buffer[aes.blockSize()];
    for (size_t i = 0; i < aes.blockSize(); i++)
    {
        buffer[i] = EEPROM.read(1 + i);
    }
    aes.decryptBlock(buffer, buffer);
    for (uint8_t i = 0; i < aes.blockSize(); i++)
    {
        if (buffer[i] != i)
        {
            return false;
        }
    }
    return true;
}

void getAccountName(int accountIndex, char *buffer)
{
    size_t accountSize = accountNameLength + 2 * aes.blockSize();
    for (int i = 0; i < accountNameLength; i++)
    {
        char c = (char)EEPROM.read(accountOffset + accountIndex * accountSize + i);
        buffer[i] = c;
        if (c == '\0')
        {
            break;
        }
    }
}

int clampAccountId(int id)
{
    while (id < 0)
        id += numAccounts;
    return id % numAccounts;
}

void displayAccounts()
{
    display.clearDisplay();

    if (numAccounts == 0)
    {
        display.setCursor(0, 16);
        display.setTextSize(2);
        display.println("No");
        display.println("Passwords.");
    }
    else
    {
        display.setCursor(0, 0);
        display.setTextSize(1);

        int start = currentAccount - 3;
        for (int i = start; i <= currentAccount + 3; i++)
        {
            if (i == currentAccount)
            {
                display.setTextSize(2);
            }
            else
            {
                display.setTextSize(1);
            }
            char buffer[accountNameLength];
            getAccountName(clampAccountId(i), buffer);
            display.println(buffer);
        }
    }

    display.display();
}

void displayWrongPassword()
{
    display.clearDisplay();
    display.setCursor(16, 24);
    display.setTextSize(2);
    display.println("wrong :(");
    display.display();

    delay(2000);
}

void typePassword()
{
    char accountName[accountNameLength];
    getAccountName(currentAccount, accountName);

    setKey(accountName);

    if (!validateKey())
    {
        aes.clear();
        displayWrongPassword();
        displayAccounts();
        return;
    }

    size_t bSize = aes.blockSize();
    size_t accountSize = accountNameLength + 2 * aes.blockSize();

    uint8_t buffer[bSize];

    for (int k = 0; k < 2; k++)
    {
        for (size_t i = 0; i < bSize; i++)
        {
            buffer[i] = EEPROM.read(accountOffset + currentAccount * accountSize + i + accountNameLength + k * bSize);
        }
        aes.decryptBlock(buffer, buffer);
        for (size_t i = 0; i < bSize; i++)
        {
            char c = buffer[i];
            if (c == '\0')
            {
                // no more blocks are needed
                k = 2;
                break;
            }
            Keyboard.print(c);
        }
    }

    aes.clear();

    displayAccounts();
}

void resetKey()
{
    setKey((char *)"reset");
    // TODO: does this pose a security risk?
    uint8_t buffer[aes.blockSize()];
    for (uint8_t i = 0; i < aes.blockSize(); i++)
    {
        buffer[i] = i;
    }
    aes.encryptBlock(buffer, buffer);
    aes.clear();

    for (size_t i = 0; i < aes.blockSize(); i++)
    {
        EEPROM.write(1 + i, buffer[i]);
    }

    // reset the accounts
    EEPROM.write(0, 0);
    numAccounts = 0;

    // clear the rest of the EEPROM
    for (size_t i = accountOffset; i < EEPROM.length(); i++)
    {
        EEPROM.write(i, 0);
    }

    displayAccounts();
}

void setup()
{
    pinMode(pinSubmit, INPUT_PULLUP);
    pinMode(pinNextAccount, INPUT_PULLUP);
    pinMode(pinPrevAccount, INPUT_PULLUP);
    Keyboard.begin();
    Serial.begin(9600);

    numAccounts = EEPROM.read(0);

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    {
        Serial.println(F("SSD1306 allocation failed"));
        // Don't proceed, loop forever
        for (;;)
            ;
    }

    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setCursor(16, 24);
    display.setTextSize(2);
    display.println("hi there");
    display.display();
    delay(2000);

    displayAccounts();
}

int findAccountIndex(String *name)
{
    for (int i = 0; i < numAccounts; i++)
    {
        char buffer[accountNameLength];
        getAccountName(i, buffer);
        String tmp(buffer);
        if (tmp.equals(*name))
        {
            return i;
        }
    }
    return -1;
}

void removeAccount(String *name)
{
    int index = findAccountIndex(name);
    if (index <= -1)
    {
        // if the account does not exist, abort
        return;
    }

    size_t accountSize = accountNameLength + 2 * aes.blockSize();
    // move the following accounts up
    for (int i = index + 1; i < numAccounts; i++)
    {
        for (size_t k = 0; k < accountSize; k++)
        {
            EEPROM.write(accountOffset + (i - 1) * accountSize + k, EEPROM.read(accountOffset + i * accountSize + k));
        }
    }

    numAccounts--;
    EEPROM.write(0, numAccounts);

    if (currentAccount == numAccounts)
    {
        currentAccount = 0;
    }

    // update the account view
    displayAccounts();
}

void storeNewPassword()
{
    int splitIndex = inputBuffer.indexOf('\x1F');
    if (splitIndex <= -1 || splitIndex == 0 || (unsigned)splitIndex == inputBuffer.length() - 1)
    {
        return;
    }
    String name = inputBuffer.substring(0, splitIndex);
    String pw = inputBuffer.substring(splitIndex + 1);

    String msg = "add " + name;
    char msgBuffer[msg.length() + 1];
    msg.toCharArray(msgBuffer, msg.length() + 1);
    setKey(msgBuffer);

    if (!validateKey())
    {
        aes.clear();
        displayWrongPassword();
        displayAccounts();
        return;
    }

    // does the account already exist?
    int insertIndex = findAccountIndex(&name);
    size_t accountSize = accountNameLength + 2 * aes.blockSize();
    if (insertIndex == -1)
    {
        insertIndex = numAccounts;
        // write new account name
        for (unsigned int i = 0; i < min(accountNameLength, name.length()); i++)
        {
            EEPROM.write(accountOffset + (accountSize * insertIndex) + i, name.charAt(i));
        }
        EEPROM.write(accountOffset + (accountSize * insertIndex) + min(accountNameLength, name.length()), '\0');
    }

    size_t pwLength = 2 * aes.blockSize();

    char chars[pwLength];
    pw.toCharArray(chars, pwLength);
    chars[pwLength - 1] = '\0';

    // write all password blocks
    uint8_t buffer[aes.blockSize()];
    for (int k = 0; k < 2; k++)
    {
        for (size_t i = 0; i < aes.blockSize(); i++)
        {
            buffer[i] = chars[i + k * aes.blockSize()];
        }
        aes.encryptBlock(buffer, buffer);
        for (size_t i = 0; i < aes.blockSize(); i++)
        {
            EEPROM.write(accountOffset + accountSize * insertIndex + accountNameLength + i + k * aes.blockSize(), buffer[i]);
        }
    }

    aes.clear();

    if (insertIndex == numAccounts)
    {
        numAccounts++;
        EEPROM.write(0, numAccounts);
    }

    displayAccounts();
}

void receiveInput()
{
    if (Serial.available())
    {
        char val = Serial.read();

        // newline marks the end of a serial transmission
        if (val == '\n')
        {
            // first character is backspace character
            if (inputBuffer.charAt(0) == '\x8')
            {
                String accountName = inputBuffer.substring(1);
                removeAccount(&accountName);
            }
            // first character is DEL
            else if (inputBuffer.charAt(0) == '\x7F')
            {
                resetKey();
            }
            else
            {
                storeNewPassword();
            }
            inputBuffer = "";
        }
        else
        {
            inputBuffer += val;
        }
    }
}

void loop()
{
    if (btnIsPressed(pinSubmit, lastSubmitState))
    {
        typePassword();
    }

    if (btnIsPressed(pinNextAccount, lastNextAccountState))
    {
        currentAccount = (currentAccount + 1) % numAccounts;
        displayAccounts();
    }

    if (btnIsPressed(pinPrevAccount, lastPrevAccountState))
    {
        currentAccount--;
        while (currentAccount < 0)
            currentAccount += numAccounts;
        displayAccounts();
    }

    receiveInput();
}
