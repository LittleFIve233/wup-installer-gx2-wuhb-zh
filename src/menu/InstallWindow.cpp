/****************************************************************************
 * Copyright (C) 2011 Dimok
 * Copyright (C) 2012 Cyan
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ****************************************************************************/
#include "Application.h"
#include "InstallWindow.h"
#include "utils/StringTools.h"
#include "common/common.h"
#include "system/power.h"
#include <coreinit/mcp.h>
#include <coreinit/memory.h>
#include <coreinit/ios.h>

#define MCP_COMMAND_INSTALL_ASYNC   0x81
#define MAX_INSTALL_PATH_LENGTH     0x27F

static int installCompleted = 0;
static u32 installError = 0;

extern "C" MCPError MCP_GetLastRawError(void);

static void* IosInstallCallback(IOSError errorCode, void * priv_data)
{
	installError = errorCode;
	installCompleted = 1;
	return 0;
}

InstallWindow::InstallWindow(CFolderList * list)
	: GuiFrame(0, 0)
	, CThread(CThread::eAttributeAffCore0 | CThread::eAttributePinnedAff)
	, folderList(list)
{   
	mainWindow = Application::instance()->getMainWindow();
	
	folderCount = folderList->GetSelectedCount();
	
	if(folderCount > 0)
	{
		std::string message = fmt("总共 %d 个软件", folderCount);
		messageBox = new MessageBox(MessageBox::BT_YESNO, MessageBox::IT_ICONQUESTION, false);
		messageBox->setTitle("你想要安装这些软件吗:");
		messageBox->setMessage1(message);
		messageBox->messageYesClicked.connect(this, &InstallWindow::OnValidInstallClick);
		messageBox->messageNoClicked.connect(this, &InstallWindow::OnCloseWindow);
	}
	else
	{
		messageBox = new MessageBox(MessageBox::BT_OK, MessageBox::IT_ICONEXCLAMATION, false);
		messageBox->setTitle("没有选中的内容。");
		messageBox->setMessage1("返回到选择界面");
		messageBox->messageOkClicked.connect(this, &InstallWindow::OnCloseWindow);
	}
	
	drcFrame = new GuiFrame(0, 0);
	drcFrame->setEffect(EFFECT_FADE, 10, 255);
	drcFrame->setState(GuiElement::STATE_DISABLED);
	drcFrame->effectFinished.connect(this, &InstallWindow::OnOpenEffectFinish);
	drcFrame->append(messageBox);
	
	mainWindow->append(drcFrame);
}

InstallWindow::~InstallWindow()
{
	drcFrame->remove(messageBox);
	mainWindow->remove(drcFrame);
	delete drcFrame;
	delete messageBox;
}

void InstallWindow::OnValidInstallClick(GuiElement * element, int val)
{
	messageBox->messageYesClicked.disconnect(this);
	messageBox->messageNoClicked.disconnect(this);
	messageBox->reload("你想安装在哪里?", "", "", MessageBox::BT_DEST, MessageBox::IT_ICONQUESTION);
	messageBox->messageYesClicked.connect(this, &InstallWindow::OnDestinationChoice);
	messageBox->messageNoClicked.connect(this, &InstallWindow::OnDestinationChoice);
}

void InstallWindow::OnDestinationChoice(GuiElement * element, int choice)
{
	if(choice == MessageBox::MR_YES)
		target = NAND;
	else
		target = USB;
	
	messageBox->messageYesClicked.disconnect(this);
	messageBox->messageNoClicked.disconnect(this);
	
	startInstalling();
}

void InstallWindow::executeThread()
{
	Application::instance()->exitDisable();
	OSEnableHomeButtonMenu(false);
	
	canceled = false;
	
	bool APD_enabled = isEnabledAutoPowerDown();
	if(APD_enabled)
		disableAutoPowerDown();
	
	int total = folderList->GetSelectedCount();
	int pos = 1;
	
	while(pos <= total && !canceled)
	{
		InstallProcess(pos, total);
		
		if(pos < total)
		{
			int time = 6;
			u64 startTime = OSGetTime();
			u32 passedMs = 0;
			
			while(time && !canceled)
			{
				passedMs = OSTicksToMilliseconds(OSGetTime() - startTime);
				
				if(passedMs >= 1000)
				{
					time--;
					startTime = OSGetTime();
					messageBox->setMessage2(fmt("%d秒后开始安装下个软件", time));
				}
			}
			
			messageBox->messageCancelClicked.disconnect(this);
		}
		
		pos++;
	}
	
	if(APD_enabled)
		enableAutoPowerDown();
	
	OSEnableHomeButtonMenu(true);
	Application::instance()->exitEnable();
}

void InstallWindow::InstallProcess(int pos, int total)
{
	int index = folderList->GetFirstSelected();
	
	std::string title = fmt("安装中... (%d/%d)", pos, total);
	std::string gameName = folderList->GetName(index);
	
	messageBox->reload(title, gameName, "", MessageBox::BT_NOBUTTON, MessageBox::IT_ICONINFORMATION, true, "0.0 %");
	
	/////////////////////////////
	// install process
	/////////////////////////////
	
	int result = 0;
	installCompleted = 0;
	installError = 0;
	
	//!---------------------------------------------------
	//! This part of code originates from Crediars MCP patcher assembly code
	//! it is just translated to C
	//!---------------------------------------------------
	unsigned int mcpHandle = MCP_Open();
	if(mcpHandle == 0)
	{
		messageBox->reload("安装失败", gameName, "无法打开MCP。", MessageBox::BT_OK, MessageBox::IT_ICONERROR);
		
		result = -1;
	}
	else
	{
        char installPath[256];
		unsigned int * mcpInstallInfo = (unsigned int *)OSAllocFromSystem(0x24, 0x40);
		char * mcpInstallPath = (char *)OSAllocFromSystem(MAX_INSTALL_PATH_LENGTH, 0x40);
		IOSVec * mcpPathInfoVector = (IOSVec *)OSAllocFromSystem(0x0C, 0x40);
		
		do
		{
			if(!mcpInstallInfo || !mcpInstallPath || !mcpPathInfoVector)
			{
				messageBox->reload("安装失败", gameName, "无法分配内存。", MessageBox::BT_OK, MessageBox::IT_ICONERROR);
				result = -2;
				break;
			}
			
			std::string installFolder = folderList->GetPath(index);
			installFolder.erase(0, 19);
			installFolder.insert(0, "/vol/app_sd/");
            
            snprintf(installPath, sizeof(installPath), "%s", installFolder.c_str());
			
			int res = MCP_InstallGetInfo(mcpHandle, installPath, (MCPInstallInfo*)mcpInstallInfo);
			if(res != 0)
			{
				//__os_snprintf(errorText1, sizeof(errorText1), "Error: MCP_InstallGetInfo 0x%08X", MCP_GetLastRawError());
				messageBox->reload(installFolder, gameName, "确认文件夹中有完整的WUP文件。", MessageBox::BT_OK, MessageBox::IT_ICONERROR);
				result = -3;
				break;
			}
			
			u32 titleIdHigh = mcpInstallInfo[0];
			u32 titleIdLow = mcpInstallInfo[1];
			bool spoofFiles = false;
			if ((titleIdHigh == 00050010)
				&&(	   (titleIdLow == 0x10041000)     // JAP Version.bin
					|| (titleIdLow == 0x10041100)     // USA Version.bin
					|| (titleIdLow == 0x10041200)))   // EUR Version.bin
			{
				spoofFiles = true;
				target = NAND;
			}
			
			if (spoofFiles
			   || (titleIdHigh == 0x0005000E)     // game update
			   || (titleIdHigh == 0x00050000)     // game
			   || (titleIdHigh == 0x0005000C)     // DLC
			   || (titleIdHigh == 0x00050002))    // Demo
			{
				res = MCP_InstallSetTargetDevice(mcpHandle, (MCPInstallTarget)(target));
				if(res != 0)
				{
					messageBox->reload("安装失败", gameName, fmt("MCP_InstallSetTargetDevice 0x%08X", MCP_GetLastRawError()), MessageBox::BT_OK, MessageBox::IT_ICONERROR);
					//if (installToUsb)
					//	__os_snprintf(errorText2, sizeof(errorText2), "Possible USB HDD disconnected or failure");
					result = -5;
					break;
				}
				res = MCP_InstallSetTargetUsb(mcpHandle, (MCPInstallTarget)(target));
				if(res != 0)
				{
					messageBox->reload("安装失败", gameName, fmt("MCP_InstallSetTargetUsb 0x%08X", MCP_GetLastRawError()), MessageBox::BT_OK, MessageBox::IT_ICONERROR);
					//if (installToUsb)
					//	__os_snprintf(errorText2, sizeof(errorText2), "Possible USB HDD disconnected or failure");
					result = -6;
					break;
				}
				
				mcpInstallInfo[2] = (unsigned int)MCP_COMMAND_INSTALL_ASYNC;
				mcpInstallInfo[3] = (unsigned int)mcpPathInfoVector;
				mcpInstallInfo[4] = (unsigned int)1;
				mcpInstallInfo[5] = (unsigned int)0;
				
				memset(mcpInstallPath, 0, MAX_INSTALL_PATH_LENGTH);
				snprintf(mcpInstallPath, MAX_INSTALL_PATH_LENGTH, installFolder.c_str());
				memset(mcpPathInfoVector, 0, 0x0C);
				
				mcpPathInfoVector->vaddr = mcpInstallPath;
				mcpPathInfoVector->len = (unsigned int)MAX_INSTALL_PATH_LENGTH;
				
				res = IOS_IoctlvAsync(mcpHandle, MCP_COMMAND_INSTALL_ASYNC, 1, 0, mcpPathInfoVector, (IOSAsyncCallbackFn)IosInstallCallback, mcpInstallInfo);
				if(res != 0)
				{
					messageBox->reload("安装失败", gameName, fmt("MCP_InstallTitleAsync 0x%08X", MCP_GetLastRawError()), MessageBox::BT_OK, MessageBox::IT_ICONERROR);
					result = -7;
					break;
				}
				
				while(!installCompleted)
				{
					memset(mcpInstallInfo, 0, 0x24);
					
					MCP_InstallGetProgress(mcpHandle, (MCPInstallProgress*)mcpInstallInfo);
					
					if(mcpInstallInfo[0] == 1)
					{
						u64 totalSize = ((u64)mcpInstallInfo[3] << 32ULL) | mcpInstallInfo[4];
						u64 installedSize = ((u64)mcpInstallInfo[5] << 32ULL) | mcpInstallInfo[6];
						int percent = (totalSize != 0) ? ((installedSize * 100.0f) / totalSize) : 0;
						
						std::string message = fmt("%0.1f / %0.1f MB (%i", installedSize / (1024.0f * 1024.0f), totalSize / (1024.0f * 1024.0f), percent);
						message += "%)";
						
						messageBox->setProgress(percent);
						messageBox->setProgressBarInfo(message);
					}
					
					usleep(50000);
				}
				
				if(installError != 0)
				{
					if ((installError == 0xFFFCFFE9) && (target == USB))
					{
						messageBox->reload("安装失败", gameName, fmt("0x%08X无法连接 (没有USB设备?)", installError), MessageBox::BT_OK, MessageBox::IT_ICONERROR);
						result = -8;
					}
					else
					{
						//__os_snprintf(errorText1, sizeof(errorText1), "Error: install error code 0x%08X", installError);
						if (installError == 0xFFFBF446 || installError == 0xFFFBF43F)
							messageBox->reload("安装失败", gameName, "没有或已损的title.tik文件?", MessageBox::BT_OK, MessageBox::IT_ICONERROR);
						else if (installError == 0xFFFBF441)
							messageBox->reload("安装失败", gameName, "DLC的title.tik可能不正确。", MessageBox::BT_OK, MessageBox::IT_ICONERROR);
						else if (installError == 0xFFFCFFE4)
							messageBox->reload("安装失败", gameName, "可能选中的设备没有足够内存。", MessageBox::BT_OK, MessageBox::IT_ICONERROR);
						else if (installError == 0xFFFFF825)
							messageBox->reload("安装失败", gameName, "SD卡可能已损坏。重新格式化(簇大小选32k)或更换SD卡。", MessageBox::BT_OK, MessageBox::IT_ICONERROR);
						else if ((installError & 0xFFFF0000) == 0xFFFB0000)
							messageBox->reload("安装失败", gameName, "检查WUP是否正确完整。数字版游戏和DLC需要Sig-Patches。", MessageBox::BT_OK, MessageBox::IT_ICONERROR);
						
						result = -9;
					}
				}
			}
			else
			{
				messageBox->reload("安装失败", gameName, "不是游戏,更新补丁,DLC,试玩版或完整的WUP。", MessageBox::BT_OK, MessageBox::IT_ICONERROR);
				result = -4;
			}
		}
		while(0);
		
		MCP_Close(mcpHandle);
		if(mcpPathInfoVector)
			OSFreeToSystem(mcpPathInfoVector);
		if(mcpInstallPath)
			OSFreeToSystem(mcpInstallPath);
		if(mcpInstallInfo)
			OSFreeToSystem(mcpInstallInfo);
	}
	/////////////////////////////
	
	if(result >= 0)
	{
		if(pos == total)
		{
			messageBox->reload("安装完成", gameName, "", MessageBox::BT_OK, MessageBox::IT_ICONTRUE);
			messageBox->messageOkClicked.connect(this, &InstallWindow::OnCloseWindow);
		}
		else
		{
			messageBox->reload("安装完成", gameName, "6秒后进行下个软件安装", MessageBox::BT_CANCEL, MessageBox::IT_ICONTRUE);
			messageBox->messageCancelClicked.connect(this, &InstallWindow::OnInstallProcessCancel);
		}
		
		folderList->UnSelect(index);
	}
	else
	{
		messageBox->messageOkClicked.connect(this, &InstallWindow::OnCloseWindow);
		
		canceled = true;
		folderList->UnSelectAll();
	}
}

void InstallWindow::OnInstallProcessCancel(GuiElement *element, int val)
{
	canceled = true;
	folderList->UnSelectAll();
	OnCloseWindow(this, 0);
}

void InstallWindow::OnCloseWindow(GuiElement * element, int val)
{
	messageBox->setEffect(EFFECT_FADE, -10, 255);
	messageBox->setState(GuiElement::STATE_DISABLED);
	messageBox->effectFinished.connect(this, &InstallWindow::OnWindowClosed);
}

void InstallWindow::OnWindowClosed(GuiElement *element)
{
	messageBox->effectFinished.disconnect(this);
	installWindowClosed(this);
	
	AsyncDeleter::pushForDelete(this);
}

void InstallWindow::OnOpenEffectFinish(GuiElement *element)
{
	element->effectFinished.disconnect(this);
	element->clearState(GuiElement::STATE_DISABLED);
}

void InstallWindow::OnCloseEffectFinish(GuiElement *element)
{
	remove(element);
	AsyncDeleter::pushForDelete(element);
}
