// pin layout
// 2 OLED-SDA
// 3 OLED-SCL
// 6 type password button
// 7 next account button
// 8 previous account button

// TODO: solve keyboard layouts
// TODO: add wrong password cooldown

#include <Arduino.h>
#include <KeyboardMultiLanguage.h>
#include "KeyboardMappingGE.h"

// OLED
#include <Adafruit_SSD1306.h>

// passwords
#include <AES.h>
#include <EEPROM.h>

#include "storage.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1 // my OLED screen has no reset pin
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

AES128 aes;

const int PIN_SUBMIT = 6;
int lastSubmitState = HIGH;

const int PIN_NEXT_ACCOUNT = 7;
int lastNextAccountState = HIGH;
const int PIN_PREV_ACCOUNT = 8;
int lastPrevAccountState = HIGH;

const int ACCOUNT_OFFSET = 1 + 16;
const int ACCOUNT_NAME_LEN = 16;
int currentAccount = 0;
uint8_t numAccounts;

String inputBuffer;

// must be true: AUTH_LEN % aes.keySize() == 0
const size_t AUTH_LEN = 8;

int accountLimit = 0;

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

int eepromAccount(int accountIndex)
{
    return ACCOUNT_OFFSET + accountIndex * (ACCOUNT_NAME_LEN + 2 * aes.blockSize());
}

int eepromPassword(int accountIndex, int block=0)
{
    return eepromAccount(accountIndex) + ACCOUNT_NAME_LEN + block * aes.blockSize();
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
        display.print(buffer[i]);
    }
    for (size_t i = progress; i < AUTH_LEN; i++)
    {
        display.print('.');
    }
    display.display();
}

bool setKey(char *info, bool validate=true)
{
    uint8_t buffer[AUTH_LEN];
    size_t index = 0;
    displayPassword(0, buffer, info);
    while (index < AUTH_LEN)
    {
        if (btnIsPressed(PIN_SUBMIT, lastSubmitState))
        {
            buffer[index] = 0x00;
            index++;
            displayPassword(index, buffer, info);
        }
        if (btnIsPressed(PIN_NEXT_ACCOUNT, lastNextAccountState))
        {
            buffer[index] = 0x01;
            index++;
            displayPassword(index, buffer, info);
        }
        if (btnIsPressed(PIN_PREV_ACCOUNT, lastPrevAccountState))
        {
            buffer[index] = 0x02;
            index++;
            displayPassword(index, buffer, info);
        }
    }

    uint8_t key[aes.keySize()];
    int repeat = aes.keySize() / AUTH_LEN;
    for (size_t i = 0; i < AUTH_LEN; i++)
    {
        for (int k = 0; k < repeat; k++)
        {
            key[i * repeat + k] = buffer[i];
        }
    }
    aes.setKey(key, aes.keySize());

    if (validate)
    {
        uint8_t valBuffer[aes.blockSize()];
        for (size_t i = 0; i < aes.blockSize(); i++)
        {
            valBuffer[i] = readEEPROM(1 + i);
        }
        aes.decryptBlock(valBuffer, valBuffer);
        for (uint8_t i = 0; i < aes.blockSize(); i++)
        {
            if (valBuffer[i] != i)
            {
                // key was incorrect, reset
                aes.clear();

                // display message
                display.clearDisplay();
                display.setCursor(16, 24);
                display.setTextSize(2);
                display.println("wrong :(");
                display.display();
                delay(2000);

                return false;
            }
        }
        return true;
    }
    // output doesn't matter if validate == false
    return false;
}


void getAccountName(int accountIndex, char *buffer)
{
    for (int i = 0; i < ACCOUNT_NAME_LEN; i++)
    {
        char c = (char)readEEPROM(eepromAccount(accountIndex) + i);
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
            display.setTextSize(i == currentAccount ? 2 : 1);
            char buffer[ACCOUNT_NAME_LEN];
            getAccountName(clampAccountId(i), buffer);
            display.println(buffer);
        }
    }

    display.display();
}

void typePassword()
{
    char accountName[ACCOUNT_NAME_LEN];
    getAccountName(currentAccount, accountName);

    if (!setKey(accountName))
    {
        displayAccounts();
        return;
    }

    size_t bSize = aes.blockSize();
    uint8_t buffer[bSize];

    char password[2 * aes.blockSize()];

    for (int k = 0; k < 2; k++)
    {
        for (size_t i = 0; i < bSize; i++)
        {
            buffer[i] = readEEPROM(eepromPassword(currentAccount, k) + i);
        }
        aes.decryptBlock(buffer, buffer);
        memcpy(password + k * bSize, buffer, bSize);
    }

    Keyboard.print(password);

    aes.clear();

    displayAccounts();
}

void resetKey()
{
    setKey((char *)"reset", false);
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
        writeEEPROM(1 + i, buffer[i]);
    }

    // reset the accounts
    writeEEPROM(0, 0);
    numAccounts = 0;

    // clear the rest of the EEPROM
    for (size_t i = ACCOUNT_OFFSET; i < EEPROM.length(); i++)
    {
        writeEEPROM(i, 0);
    }

    displayAccounts();
}

void setup()
{
    pinMode(PIN_SUBMIT, INPUT_PULLUP);
    pinMode(PIN_NEXT_ACCOUNT, INPUT_PULLUP);
    pinMode(PIN_PREV_ACCOUNT, INPUT_PULLUP);
    Keyboard.language(German);
    Serial.begin(9600);

    numAccounts = readEEPROM(0);

    accountLimit = (EEPROM.length() - 17) / (ACCOUNT_NAME_LEN + 2 * aes.blockSize());

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

void removeAccount(String *name)
{
    int accountIndex = -1;
    // find the index of the account
    for (int i = 0; i < numAccounts; i++)
    {
        char buffer[ACCOUNT_NAME_LEN];
        getAccountName(i, buffer);
        String tmp(buffer);
        if (tmp.equals(*name))
        {
            accountIndex = i;
            break;
        }
    }

    if (accountIndex == -1)
    {
        // if the account does not exist, abort
        return;
    }

    size_t accountSize = ACCOUNT_NAME_LEN + 2 * aes.blockSize();
    // move the following accounts up
    for (int i = eepromAccount(accountIndex); i < eepromAccount(numAccounts); i++)
    {
        writeEEPROM(i, readEEPROM(i + accountSize));
    }

    numAccounts--;
    writeEEPROM(0, numAccounts);

    if (currentAccount == numAccounts)
    {
        currentAccount = 0;
    }

    // update the account view
    displayAccounts();
}

void storeNewPassword()
{
    if (numAccounts >= accountLimit)
    {
        return;
    }
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
    if (!setKey(msgBuffer))
    {
        displayAccounts();
        return;
    }

    int insertIndex = 0;
    bool accountExists = false;

    for (int i = numAccounts - 1; i >= 0; i--)
    {
        char buffer[ACCOUNT_NAME_LEN];
        getAccountName(i, buffer);
        String tmp(buffer);
        int comp = name.compareTo(tmp);

        if (comp == 0)
        {
            insertIndex = i;
            accountExists = true;
            break;
        }
        else if (comp > 0)
        {
            insertIndex = i + 1;
            accountExists = false;
            break;
        }
    }

    if (!accountExists)
    {
        // shift accounts to the right
        for (int i = eepromAccount(numAccounts) - 1; i >= eepromAccount(insertIndex); i--)
        {
            writeEEPROM(i + ACCOUNT_NAME_LEN + 2 * aes.blockSize(), readEEPROM(i));
        }

        numAccounts++;
        writeEEPROM(0, numAccounts);

        // write new account name
        for (unsigned int k = 0; k < min(ACCOUNT_NAME_LEN, name.length()); k++)
        {
            writeEEPROM(eepromAccount(insertIndex) + k, name.charAt(k));
        }
        writeEEPROM(eepromAccount(insertIndex) + min(ACCOUNT_NAME_LEN, name.length()), '\0');
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
            writeEEPROM(eepromPassword(insertIndex, k) + i, buffer[i]);
        }
    }

    aes.clear();

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
    if (btnIsPressed(PIN_SUBMIT, lastSubmitState))
    {
        typePassword();
    }

    if (btnIsPressed(PIN_NEXT_ACCOUNT, lastNextAccountState))
    {
        currentAccount = (currentAccount + 1) % numAccounts;
        displayAccounts();
    }

    if (btnIsPressed(PIN_PREV_ACCOUNT, lastPrevAccountState))
    {
        currentAccount--;
        while (currentAccount < 0)
            currentAccount += numAccounts;
        displayAccounts();
    }

    receiveInput();
}
