# Node.js Uninstallation Guide

This guide provides detailed steps to completely uninstall Node.js from your system using
different package managers and methods. Select the method that matches how you installed Node.js.

## Table of Contents

<!--lint disable prohibited-strings-->

1. [Homebrew](#homebrew)
2. [Node Version Manager (nvm)](#node-version-manager-nvm)
3. [Node Version Management (n)](#node-version-management-n)
4. [Fast Node Manager (fnm)](#fast-node-manager-fnm)
5. [Chocolatey](#chocolatey)
6. [Docker](#docker)
7. [MacOS Prebuilt Installer](#macos-prebuilt-installer)
8. [Windows Prebuilt Installer](#windows-prebuilt-installer)
9. [Manual Installation](#manual-installation)

<!--lint enable prohibited-strings-->

***

## Homebrew

### Removing Node.js Installed via Homebrew

1. **Launch Terminal.**
2. **Execute the command to uninstall Node.js:**
   ```bash
   brew uninstall node
   ```
3. **Clear Homebrew cache:**
   ```bash
   brew cleanup
   ```
4. **Check that Node.js is removed:**
   ```bash
   node -v
   ```
   An error message will confirm Node.js is uninstalled.

***

<!--lint disable prohibited-strings-->

## Node Version Manager (nvm)

<!--lint enable prohibited-strings-->

### Removing Node.js with nvm

1. **Open Terminal.**
2. **Show all installed Node.js versions:**
   ```bash
   nvm ls
   ```
3. **Remove a specific Node.js version:**
   Replace `version` with the version number you want to uninstall.
   ```bash
   nvm uninstall <version>
   ```
   Example:
   ```bash
   nvm uninstall 14.17.0
   ```
4. **Optional: Remove nvm entirely:**
   ```bash
   rm -rf ~/.nvm
   ```
   Update your shell profile (`~/.bashrc`, `~/.zshrc`, etc.) to delete nvm references.
5. **Verify Node.js is removed:**
   ```bash
   node -v
   ```
   An error message will confirm Node.js is uninstalled.

***

<!--lint disable prohibited-strings-->

## Node Version Management (n)

<!--lint enable prohibited-strings-->

### Removing Node.js with n

1. **Open Terminal.**
2. **Display installed Node.js versions:**
   ```bash
   n ls
   ```
3. **Uninstall a specific Node.js version:**
   Replace `version` with the version number you want to remove.
   ```bash
   sudo n rm <version>
   ```
   Example:
   ```bash
   sudo n rm 14.17.0
   ```
4. **Optional: Uninstall n:**
   ```bash
   sudo npm uninstall -g n
   ```
5. **Ensure Node.js is uninstalled:**
   ```bash
   node -v
   ```
   An error message will confirm Node.js is uninstalled.

***

<!--lint disable prohibited-strings-->

## Fast Node Manager (fnm)

<!--lint enable prohibited-strings-->

### Removing Node.js with fnm

1. **Open Terminal.**
2. **List all Node.js versions managed by fnm:**
   ```bash
   fnm list
   ```
3. **Uninstall a specific version:**
   Replace `version` with the version number you wish to remove.
   ```bash
   fnm uninstall <version>
   ```
   Example:
   ```bash
   fnm uninstall 14.17.0
   ```
4. **Optional: Remove fnm entirely:**
   ```bash
   rm -rf ~/.fnm
   ```
   Update your shell profile (`~/.bashrc`, `~/.zshrc`, etc.) to delete fnm references.
5. **Verify Node.js removal:**
   ```bash
   node -v
   ```
   An error message will confirm Node.js is uninstalled.

***

## Chocolatey

### Uninstalling Node.js with Chocolatey

1. **Open an elevated Command Prompt or PowerShell (Run as Administrator).**
2. **Uninstall Node.js:**
   ```bash
   choco uninstall nodejs
   ```
3. **Clean up Chocolatey:**
   ```bash
   choco clean
   ```
4. **Confirm Node.js removal:**
   ```bash
   node -v
   ```
   An error message will confirm Node.js is uninstalled.

***

## Docker

### Removing Node.js Docker Containers and Images

1. **Open Terminal or Command Prompt.**
2. **List all running containers:**
   ```bash
   docker ps
   ```
3. **Stop the Node.js container:**
   Replace `container_id` with the appropriate container ID or name.
   ```bash
   docker stop <container_id>
   ```
4. **Remove the container:**
   ```bash
   docker rm <container_id>
   ```
5. **Optional: Remove the Node.js Docker image:**
   Replace `image_id` with the appropriate image ID or name.
   ```bash
   docker rmi <image_id>
   ```
6. **Verify image removal:**
   ```bash
   docker images
   ```

***

## macOS prebuilt installer

### Removing Node.js installed via prebuilt installer on macOS

1. **Open Terminal.**
2. **Delete Node.js and npm:**
   ```bash
   sudo rm -rf /usr/local/lib/node_modules /usr/local/include/node /usr/local/bin/node /usr/local/bin/npm
   ```
3. **Optional: Remove global npm modules:**
   ```bash
   sudo rm -rf /usr/local/lib/node_modules
   ```
4. **Verify Node.js removal:**
   ```bash
   node -v
   ```
   An error message will confirm Node.js is uninstalled.

***

## Windows Prebuilt Installer

### Removing Node.js Installed via Prebuilt Installer on Windows

1. **Open Control Panel.**
2. **Navigate to Programs and Features:**
   * Press `Windows + R`, type `appwiz.cpl`, and press Enter.
3. **Locate Node.js in the list of programs.**
4. **Select Node.js and click Uninstall.**
5. **Follow the uninstallation wizard.**
6. **Confirm Node.js removal:**
   Open Command Prompt and run:
   ```bash
   node -v
   ```
   An error message will confirm Node.js is uninstalled.

***

## Manual Installation

### Removing Node.js Manually

#### On MacOS or Linux:

1. **Open Terminal.**
2. **Delete Node.js and npm:**
   ```bash
   sudo rm -rf /usr/local/bin/node /usr/local/bin/npm /usr/local/lib/node_modules /usr/local/include/node /usr/local/share/man/man1/node.1
   ```
3. **Optional: Remove global npm modules:**
   ```bash
   sudo rm -rf /usr/local/lib/node_modules
   ```
4. **Verify Node.js removal:**
   ```bash
   node -v
   ```
   An error message will confirm Node.js is uninstalled.

#### On Windows:

1. **Open Command Prompt as Administrator.**
2. **Delete Node.js and npm directories:**
   ```bash
   rmdir /S /Q "C:\Program Files\nodejs"
   ```
3. **Optional: Remove Node.js from the system PATH:**
   * Press `Windows + R`, type `sysdm.cpl`, and press Enter.
   * Go to the **Advanced** tab and click **Environment Variables**.
   * In **System variables**, select **Path** and click **Edit**.
   * Remove Node.js and npm entries.
4. **Verify Node.js removal:**
   ```bash
   node -v
   ```
   An error message will confirm Node.js is uninstalled.

***

By following these instructions for your specific installation method, you can successfully remove Node.js
from your system. For further assistance, refer to the documentation or support resources for the specific tool you used.
