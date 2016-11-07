/*
    Initial author: (https://github.com/)Convery for Ayria.se
    License: MIT
    Started: 2016-10-20
    Notes:
        Reads and writes CSV files.
*/

#include "CSVManager.h"
#include "../../STDInclude.h"
#include "../Strings/Variadicstring.h"

// Reimplementation of standard functionality.
#ifdef _WIN32
char *strsep(char **String, const char *Delimiter)
{
    char *Start = *String;
    char *Part;

    Part = (Start != NULL) ? strpbrk(Start, Delimiter) : NULL;

    if (Part == NULL)
        *String = NULL;
    else
    {
        *Part = '\0';
        *String = Part + 1;
    }

    return Start;
}
#endif

std::vector<std::vector<std::string>> AYRIA::CSV::Readfile(std::string Filepath)
{
    std::vector<std::vector<std::string>> Result;
    size_t Linecount{ 0 };

    // Ensure that the file exists.
    auto Filehandle = fopen(Filepath.c_str(), "r");
    if (!Filehandle) return Result;

    // For each string in the file.
    char *Inputstring = new char [1024];
    char *Backupstring = Inputstring;
    while (fgets(Inputstring, 1024, Filehandle))
    {
        // If the line is empty, skip.
        if (std::strlen(Inputstring) < 2)
            continue;

        // If the line is a comment, skip.
        if (*Inputstring == '#')
            continue;

        // Create a new row in the buffer.
        Result.push_back(std::vector<std::string>());
        Linecount++;

        // Split the string into tokens.
        while (true)
        {
            // Get next token.
            auto Tempstring = strsep(&Inputstring, ",\n\0");

            // Add the token or quit.
            if (Tempstring != nullptr)
            {
                // Remove any spaces before the value.
                while (*Tempstring == ' ')
                    Tempstring++;

                // Append the token and a null string if it's the end of the line.
                Result[Linecount - 1].push_back(Tempstring);
            }                
            else
                break;
        }

        // Clear the buffer.
        Inputstring = Backupstring;
        memset(Inputstring, 0, 1024);
    }

    delete[] Inputstring;
    fclose(Filehandle);
    return Result;
}
bool AYRIA::CSV::Writefile(std::vector<std::vector<std::string>> Buffer, std::string Filepath)
{
    std::FILE *Filehandle{ nullptr };

    // Check that we have anything to write.
    if (Buffer.size() == 0)
        return false;

    // Create the file.
    Filehandle = fopen(Filepath.c_str(), "w");
    if (!Filehandle) return false;

    // Create the header.
    fputs("# This file is generated via Ayrias CSV manager.\n", Filehandle);
    fputs("# Layout:", Filehandle);
    for (size_t i = 0; i < Buffer[0].size(); ++i)
        fputs(AYRIA::va(" %c,", 0x41 + i), Filehandle);
    fputs("\n\n", Filehandle);

    // For each row.
    for (auto Iterator = Buffer.begin(); Iterator != Buffer.end(); ++Iterator)
    {
        for (auto Iterator2 = Iterator->begin(); Iterator2 != Iterator->end(); ++Iterator2)
            fputs(AYRIA::va(" %s,", Iterator2->c_str()), Filehandle);
        fputs("\n", Filehandle);
    }

    fclose(Filehandle);
    return true;
}

bool AYRIA::CSV::Manager::Readfile(std::string Filepath)
{
    Buffer = CSV::Readfile(Filepath);
    return Buffer.size() > 0;
}
bool AYRIA::CSV::Manager::Writefile(std::string Filepath)
{
    return CSV::Writefile(Buffer, Filepath);
}

std::string AYRIA::CSV::Manager::Value(size_t Row, size_t Col)
{
    // Check rowcount.
    if (Row > Buffer.size() - 1)
        return "";

    // Check colcount.
    if (Col > Buffer[Row].size() - 1)
        return "";

    return Buffer[Row][Col];
}
void AYRIA::CSV::Manager::Insert(std::vector<std::string> Line)
{
    Buffer.push_back(Line);
}
