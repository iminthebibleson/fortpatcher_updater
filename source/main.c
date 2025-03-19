#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>
#include <curl/curl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <zzip/zzip.h>

#define PATCHES_URL "https://github.com/YoshiCrystal9/FortPatcher-NX/releases/latest/download/all_patches.zip"
#define OUTPUT_ZIP "sdmc:/all_patches.zip"
#define EXTRACT_PATH "sdmc:/"


// Function to check if the console is connected to the internet
bool check_internet_connection()
{
    CURL *curl = curl_easy_init();
    if (!curl)
    {
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, "https://example.com");
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);  // Don't download the body
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);  // Follow redirects if needed

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK)
    {
        curl_easy_cleanup(curl);
        return false;
    }

    long response_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    curl_easy_cleanup(curl);
    return (response_code == 200);  // Only check for HTTP 200 OK
}

// Function to download patches
size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    return fwrite(ptr, size, nmemb, stream);
}

bool download_patches()
{
    CURL *curl = curl_easy_init();
    if (!curl)
    {
        printf("\x1b[31mFailed to initialize cURL\x1b[0m\n");
        return false;
    }

    // Remove existing zip file if it exists
    if (remove(OUTPUT_ZIP) == 0)
    {
        printf("\x1b[33mExisting zip file removed: %s\x1b[0m\n", OUTPUT_ZIP);
    }

    FILE *file = fopen(OUTPUT_ZIP, "wb");
    if (!file)
    {
        printf("\x1b[31mFailed to open file for writing: %s, errno: %d\x1b[0m\n", OUTPUT_ZIP, errno);
        curl_easy_cleanup(curl);
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, PATCHES_URL);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data); // Custom write function
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);           // Write to the opened file
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);        // Follow redirects
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);        // Disable SSL verification

    printf("\x1b[32mStarting download...\x1b[0m\n");

    int attempts = 0;
    CURLcode res = CURLE_OK;

    while (attempts < 5)
    {
        res = curl_easy_perform(curl);

        if (res == CURLE_OK)
        {
            printf("\x1b[32mDownload completed successfully\x1b[0m\n");
            break;
        }
        else
        {
            printf("\x1b[33mDownload attempt %d failed with error: %s\x1b[0m\n", attempts + 1, curl_easy_strerror(res));

            if (res == CURLE_COULDNT_CONNECT || res == CURLE_OPERATION_TIMEDOUT)
            {
                printf("\x1b[33mNetwork error occurred. Retrying...\x1b[0m\n");
                attempts++;
                sleep(3);
            }
            else
            {
                break;
            }
        }

        // Update console in real-time
        consoleUpdate(NULL);
    }

    curl_easy_cleanup(curl);
    fclose(file);

    if (res != CURLE_OK)
    {
        printf("\x1b[31mDownload failed. Please try again.\x1b[0m\n");
        return false;
    }

    printf("\x1b[32mDownload successful!\x1b[0m\n");
    return true;
}

// Function to ensure a directory exists before extraction
bool create_directory_if_not_exists(const char *path)
{
    struct stat st = {0};
    if (stat(path, &st) == -1)
    {
        if (mkdir(path, 0777) == 0)
        {
            printf("\x1b[36mDirectory created: %s\x1b[0m\n", path);
            return true;
        }
        else    
        {
            printf("\x1b[31mFailed to create directory: %s\x1b[0m\n", path);
            return false;
        }
    }
    return true;
}

// Function to extract patches
bool extract_patches()
{
    // Ensure the main patches directory exists
    if (!create_directory_if_not_exists(EXTRACT_PATH))
    {
        return false;
    }

    ZZIP_DIR *zip = zzip_opendir(OUTPUT_ZIP);
    if (!zip)
    {
        printf("\x1b[31mFailed to open ZIP file: %s\x1b[0m\n", OUTPUT_ZIP);
        return false;
    }

    ZZIP_DIRENT *dir_entry;
    size_t total_files = 0;

    // First pass to count the total number of files
    while ((dir_entry = zzip_readdir(zip)) != NULL)
    {
        total_files++;
    }

    // Go back to the start of the directory
    zzip_rewinddir(zip);

    size_t files_processed = 0;
    printf("\x1b[36mExtracting Patches...\x1b[0m\n");

    while ((dir_entry = zzip_readdir(zip)) != NULL)
    {
        const char *entry_name = dir_entry->d_name;

        char file_path[256];
        snprintf(file_path, sizeof(file_path), "%s/%s", EXTRACT_PATH, entry_name);

        // Check if it's a directory (based on the entry name)
        if (strchr(entry_name, '/') != NULL)
        {
            char dir_path[256];
            strncpy(dir_path, file_path, sizeof(dir_path));
            char *last_slash = strrchr(dir_path, '/');
            if (last_slash != NULL)
            {
                *last_slash = '\0';  // Remove the file name to leave just the directory path
                create_directory_if_not_exists(dir_path);
            }
        }

        // Now extract the file
        ZZIP_FILE *in = zzip_file_open(zip, entry_name, 0);
        if (in)
        {
            FILE *out = fopen(file_path, "wb");
            if (out)
            {
                char buffer[8192];
                ssize_t size;
                while ((size = zzip_read(in, buffer, sizeof(buffer))) > 0)
                {
                    fwrite(buffer, 1, size, out);
                }
                fclose(out);
            }
            zzip_close(in);
        }

        // Update progress bar
        files_processed++;
        int progress = (files_processed * 100) / total_files;
        printf("\r\x1b[32m[%-50s] %d%%\x1b[0m", 
               "##################################################" + (50 - progress), progress);

        // Flush the output to make it update in real time
        fflush(stdout);
    }

    zzip_closedir(zip);
    printf("\n"); // Move to the next line after progress bar
    return true;
}

// Main program entrypoint
int main(int argc, char *argv[])
{
    consoleInit(NULL);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    spsmInitialize();

    // Initialize sockets
    socketInitializeDefault();

    // Check if the console is connected to the internet
    if (!check_internet_connection())
    {
        printf("\x1b[31m===== FortPatcher-NX Updater | Made by: Iminthebibleson =====\x1b[0m\n\n");
        printf("\x1b[31mNo internet connection. Please connect to the internet.\x1b[0m\n");
        consoleUpdate(NULL);
        while (appletMainLoop())
        {
            padUpdate(&pad);
            u64 kDown = padGetButtonsDown(&pad);
            if (kDown & HidNpadButton_Plus)
            {
                break;
            }
            consoleUpdate(NULL);
        }
        consoleExit(NULL);
        socketExit();
        return 0;
    }
    printf("\x1b[31m===== FortPatcher-NX Updater | Made by: Iminthebibleson =====\x1b[0m\n\n");
    printf("\x1b[32mPress A to download and update patches\x1b[0m\n");
    printf("\x1b[32mPress B to Leave\x1b[0m\n");
    printf("\x1b[32mPress + to reboot (not required)\x1b[0m\n");
    consoleUpdate(NULL);

    while (appletMainLoop())
    {
        padUpdate(&pad);
        u64 kDown = padGetButtonsDown(&pad);

        if (kDown & HidNpadButton_A)
        {
            printf("\x1b[33mDownloading patches...\x1b[0m\n");

            if (download_patches())
            {
                printf("\x1b[33mDownload successful. Extracting patches...\x1b[0m\n");

                if (extract_patches())
                {
                    printf("\x1b[32mUpdate successful!\x1b[0m\n");
                }
                else
                {
                    printf("\x1b[31mError: Extraction failed.\x1b[0m\n");
                }
            }
            else
            {
                printf("\x1b[31mError: Download failed.\x1b[0m\n");
            }
        }

        if (kDown & HidNpadButton_B)
        {
            printf("\x1b[36mLeaving...\x1b[0m\n");
            break;
        }

        if (kDown & HidNpadButton_Plus)
        {
            printf("\x1b[36mRebooting...\x1b[0m\n");

            spsmShutdown(true);
            break;
        }

        consoleUpdate(NULL);
    }

    // Clean up and exit
    consoleExit(NULL);
    socketExit();
    return 0;
}
