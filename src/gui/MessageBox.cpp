/****************************************************************************
 * Copyright (C) 2015 Dimok
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
#include "MessageBox.h"
#include "utils/StringTools.h"

MessageBox::MessageBox(int typeButtons, int typeIcons, bool progressbar)
	: GuiFrame(0, 0)
	, bgBlur(1280, 720, (GX2Color){0, 0, 0, 255})
    , buttonClickSound(Resources::GetSound("button_click.mp3"))
    , boxImageData(Resources::GetImageData("messageBox.png"))
    , boxImage(boxImageData)
    , touchTrigger(GuiTrigger::CHANNEL_1, GuiTrigger::VPAD_TOUCH)
    //, wpadTouchTrigger(GuiTrigger::CHANNEL_2 | GuiTrigger::CHANNEL_3 | GuiTrigger::CHANNEL_4 | GuiTrigger::CHANNEL_5, GuiTrigger::BUTTON_A)
    , buttonATrigger(GuiTrigger::CHANNEL_ALL, GuiTrigger::BUTTON_A, true)
    , buttonBTrigger(GuiTrigger::CHANNEL_ALL, GuiTrigger::BUTTON_B, true)
    , buttonLeftTrigger(GuiTrigger::CHANNEL_ALL, GuiTrigger::BUTTON_LEFT | GuiTrigger::STICK_L_LEFT, true)
    , buttonRightTrigger(GuiTrigger::CHANNEL_ALL, GuiTrigger::BUTTON_RIGHT | GuiTrigger::STICK_L_RIGHT, true)
	, buttonImageData(Resources::GetImageData("messageBoxButton.png"))
    //, buttonHighlightedImageData(Resources::GetImageData("messageBoxButtonSelected.png"))
	, bgImageData(Resources::GetImageData("progressBar.png"))
    , bgImage(bgImageData)
    , progressImageBlack(bgImage.getWidth(), bgImage.getHeight(), (GX2Color){0, 0, 0, 255})
    , progressImageColored(bgImage.getWidth(), bgImage.getHeight(), (GX2Color){0, 0, 0, 255})
    , DPADButtons(boxImage.getWidth(),boxImage.getHeight())
	, buttonCount(-1)
	, progressBar(progressbar)
{   
	selectedButtonDPAD = -1;
	
	bgBlur.setAlpha(0.5f);
    append(&bgBlur);
	
	titleText.setColor(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    titleText.setFontSize(48);
	titleText.setAlignment(ALIGN_CENTER | ALIGN_MIDDLE);
	titleText.setParent(&boxImage);
    titleText.setPosition(0, 100);
	titleText.setMaxWidth(boxImage.getWidth() - 50.0f, GuiText::WRAP);
    titleText.setBlurGlowColor(5.0f, glm::vec4(0.0, 0.0, 0.0f, 1.0f));
	
	messageText.setColor(glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
    messageText.setFontSize(38);
	messageText.setAlignment(ALIGN_CENTER | ALIGN_MIDDLE);
    messageText.setPosition(0, 0);
	messageText.setParent(&boxImage);
	messageText.setMaxWidth(boxImage.getWidth() - 50.0f, GuiText::WRAP);
    messageText.setBlurGlowColor(5.0f, glm::vec4(0.0, 0.0, 0.0f, 1.0f));
	
	// append on top
	this->append(&boxImage);
	this->append(&titleText);
    this->append(&messageText);
	
	iconImageData = NULL;
	iconImage = NULL;
	
	setIcon(typeIcons);
	setButtons(typeButtons);
	
	infoText.setColor(glm::vec4(0.8f, 0.8f, 0.8f, 1.0f));
	infoText.setAlignment(ALIGN_CENTER | ALIGN_MIDDLE);
    infoText.setFontSize(28);
	infoText.setParent(&bgImage);
    infoText.setPosition(0, -2);
    infoText.setBlurGlowColor(5.0f, glm::vec4(0.0, 0.0, 0.0f, 1.0f));
	
	progressImageColored.setPosition(-450, 0);
	progressImageColored.setAlignment(ALIGN_CENTER | ALIGN_LEFT);
    progressImageColored.setImageColor((GX2Color){ 42, 159, 217, 255}, 0);
    progressImageColored.setImageColor((GX2Color){ 42, 159, 217, 255}, 1);
    progressImageColored.setImageColor((GX2Color){ 13, 104, 133, 255}, 2);
    progressImageColored.setImageColor((GX2Color){ 13, 104, 133, 255}, 3);
	
    setProgress(0.0f);
    
	progressFrame.setPosition(0, -160);
	progressFrame.append(&progressImageBlack);
	progressFrame.append(&progressImageColored);
	progressFrame.append(&bgImage);
	progressFrame.append(&infoText);
	
	if(progressBar)
		this->append(&progressFrame);
	
	DPADButtons.setTrigger(&buttonATrigger);
    DPADButtons.setTrigger(&buttonBTrigger);
    DPADButtons.setTrigger(&buttonLeftTrigger);
    DPADButtons.setTrigger(&buttonRightTrigger);
    DPADButtons.clicked.connect(this, &MessageBox::OnDPADClick);
	
	this->append(&DPADButtons);
}

MessageBox::~MessageBox()
{	
	for(int i = 0; i < buttonCount; i++)
    {
        this->remove(messageButtons[i].messageButton);
		delete messageButtons[i].messageButtonImg;
        //delete messageButtons[i].messageButtonHighlightedImg;
        delete messageButtons[i].messageButton;
        delete messageButtons[i].messageButtonText;
    }
	
	if(iconImage)
	{
		this->remove(iconImage);
		delete iconImage;
		Resources::RemoveImageData(iconImageData);
	}
	
	if(progressBar)
		this->remove(&progressFrame);
	
	Resources::RemoveImageData(boxImageData);
	Resources::RemoveImageData(buttonImageData);
    Resources::RemoveSound(buttonClickSound);
	Resources::RemoveImageData(bgImageData);
}

void MessageBox::reload(std::string title, std::string message, int typeButtons, int typeIcons, bool progressbar, std::string pbInfo)
{
	newButtonsType = typeButtons;
	newIconType = typeIcons;
	newProgressBar = progressbar;
	newTitle = title;
	newMessage = message;
	newInfo = pbInfo;
	
	this->setState(GuiElement::STATE_DISABLED);
	
	if(progressBar)
		progressFrame.setEffect(EFFECT_FADE, -10, 255);
	
	titleText.setEffect(EFFECT_FADE, -10, 255);
	messageText.setEffect(EFFECT_FADE, -10, 255);
	if(iconImage)
		iconImage->setEffect(EFFECT_FADE, -10, 255);
	for(int i = 0; i < buttonCount; i++)
		messageButtons[i].messageButton->setEffect(EFFECT_FADE, -10, 255);
	
	titleText.effectFinished.connect(this, &MessageBox::OnReloadFadeOutFinished);
}

void MessageBox::OnReloadFadeOutFinished(GuiElement * element)
{
	titleText.effectFinished.disconnect(this);
	
	if(progressBar)
		this->remove(&progressFrame);
	
	setTitle(newTitle);
	setMessage(newMessage);
	setIcon(newIconType);
	setButtons(newButtonsType);
	setProgressBarInfo(newInfo);
	
	progressBar = newProgressBar;
	if(progressBar)
	{
		progressFrame.setEffect(EFFECT_FADE, 10, 255);
		this->append(&progressFrame);
	}
	setProgress(0.0f);
    
	titleText.setEffect(EFFECT_FADE, 10, 255);
	messageText.setEffect(EFFECT_FADE, 10, 255);
	if(newIconType != IT_NOICON)
		iconImage->setEffect(EFFECT_FADE, 10, 255);
	if(newButtonsType != BT_NOBUTTON)
	{
		for(int i = 0; i < buttonCount; i++)
			messageButtons[i].messageButton->setEffect(EFFECT_FADE, 10, 255);
	}
	
	titleText.effectFinished.connect(this, &MessageBox::OnReloadFadeInFinished);
}

void MessageBox::OnReloadFadeInFinished(GuiElement * element)
{
	titleText.effectFinished.disconnect(this);
	this->resetState();
}

void MessageBox::setTitle(const std::string & title)
{
	titleText.setText(title.c_str());
}

void MessageBox::setMessage(const std::string & message)
{
	messageText.setText(message.c_str());
}

void MessageBox::setIcon(int typeIcons)
{
	if(iconImage)
	{
		this->remove(iconImage);
		delete iconImage;
		Resources::RemoveImageData(iconImageData);
		iconImageData = NULL;
		iconImage = NULL;
	}
	
	if (typeIcons != IT_NOICON)
	{
		switch(typeIcons)
		{
			case IT_ICONTRUE:{
				iconImageData = Resources::GetImageData("validIcon.png");
				break;
			}
			case IT_ICONERROR:{
				iconImageData = Resources::GetImageData("errorIcon.png");
				break;
			}
			case IT_ICONINFORMATION:{
				iconImageData = Resources::GetImageData("informationIcon.png");
				break;
			}
			case IT_ICONQUESTION:{
				iconImageData = Resources::GetImageData("questionIcon.png");
				break;
			}
			case IT_ICONEXCLAMATION:{
				iconImageData = Resources::GetImageData("exclamationIcon.png");
				break;
			}
			case IT_ICONWARNING:{
				iconImageData = Resources::GetImageData("warningIcon.png");
				break;
			}
		}
		
		iconImage = new GuiImage(iconImageData);
		iconImage->setPosition(0,220);
        iconImage->setParent(&boxImage);
		
		this->append(iconImage);
	}
}

void MessageBox::setButtons(int typeButtons)
{
	//this->setState(STATE_DISABLED);
	//this->buttonsUpdated.connect(this, &MessageBox::OnButtonsUpdated);
	
	log_printf("test 1\n");
    for(int i = 0; i < buttonCount; i++)
    {
        log_printf("test 1.1\n");
		messageButtons[i].messageButton->setState(STATE_DEFAULT);
		messageButtons[i].messageButton->clicked.disconnect(this);
		this->remove(messageButtons[i].messageButton);
		delete messageButtons[i].messageButtonImg;
        //delete messageButtons[i].messageButtonHighlightedImg;
        delete messageButtons[i].messageButton;
        delete messageButtons[i].messageButtonText;
    }
	messageButtons.clear();
	buttonCount = -1;
	
	log_printf("test 2\n");
    const std::string ButtonString[] =
	{
		"Ok",
		"Cancel",
		"Yes",
		"No",
		"NAND",
		"USB"
	};
	
	log_printf("test 3\n");
    if(typeButtons != BT_NOBUTTON)
	{
		selectedButton = 0;
		
		typeButtons > 0 ? buttonCount = 2 : buttonCount = 1;
		messageButtons.resize(buttonCount);
		
		log_printf("test 3.1 buttoncount = %d\n", buttonCount);
		for(int i = 0; i < buttonCount; i++)
		{      
			log_printf("test 3.2\n");
			messageButtons[i].messageButtonImg = new GuiImage(buttonImageData);
			//messageButtons[i].messageButtonHighlightedImg = new GuiImage(buttonHighlightedImageData);
			messageButtons[i].messageButton = new GuiButton(messageButtons[i].messageButtonImg->getWidth(), messageButtons[i].messageButtonImg->getHeight());
			if(typeButtons != BT_OKCANCEL && typeButtons != BT_USB)
				messageButtons[i].messageButtonText = new GuiText(ButtonString[typeButtons + i].c_str(), 42, glm::vec4(0.9f, 0.9f, 0.9f, 1.0f));
			else if(typeButtons == BT_OKCANCEL)
				messageButtons[i].messageButtonText = new GuiText(ButtonString[typeButtons - 1 + i].c_str(), 42, glm::vec4(0.9f, 0.9f, 0.9f, 1.0f));
			else
				messageButtons[i].messageButtonText = new GuiText(ButtonString[typeButtons + 1 + i].c_str(), 42, glm::vec4(0.9f, 0.9f, 0.9f, 1.0f));//nand:yes, usb:no
			
			messageButtons[i].messageButtonText->setPosition(0, -10);
			//messageButtons[i].messageButton->setImageSelectOver(messageButtons[i].messageButtonHighlightedImg);
			messageButtons[i].messageButton->setLabel(messageButtons[i].messageButtonText);
			messageButtons[i].messageButton->setSoundClick(buttonClickSound);
			messageButtons[i].messageButton->setImage(messageButtons[i].messageButtonImg);
			messageButtons[i].messageButton->setParent(&boxImage);
			
			switch(typeButtons)
			{
				case BT_OK:{
					messageButtons[i].messageButton->clicked.connect(this, &MessageBox::OnOkButtonClick);
					messageButtons[i].messageButton->setPosition(0, -240);
					break;
				}
				case BT_OKCANCEL:{
					i == 0 ? messageButtons[i].messageButton->clicked.connect(this, &MessageBox::OnOkButtonClick) : messageButtons[i].messageButton->clicked.connect(this, &MessageBox::OnCancelButtonClick);
					messageButtons[i].messageButton->setPosition(- 220 + (messageButtons[i].messageButtonImg->getWidth()) * i , - 240);
					break;
				}
				case BT_YESNO:{
					i == 0 ? messageButtons[i].messageButton->clicked.connect(this, &MessageBox::OnYesButtonClick) : messageButtons[i].messageButton->clicked.connect(this, &MessageBox::OnNoButtonClick);
					messageButtons[i].messageButton->setPosition(- 220 + (messageButtons[i].messageButtonImg->getWidth()) * i , - 240);
					break;
				}
				case BT_USB:{
					i == 0 ? messageButtons[i].messageButton->clicked.connect(this, &MessageBox::OnYesButtonClick) : messageButtons[i].messageButton->clicked.connect(this, &MessageBox::OnNoButtonClick);
					messageButtons[i].messageButton->setPosition(- 220 + (messageButtons[i].messageButtonImg->getWidth()) * i , - 240);
					break;
				}
			}
			
			messageButtons[i].messageButton->setEffectGrow();
			messageButtons[i].messageButton->setTrigger(&touchTrigger);
			//messageButtons[i].messageButton->setTrigger(&wpadTouchTrigger);
			this->append(messageButtons[i].messageButton);
		}
	}
	log_printf("test end\n");
	
	//buttonsUpdated();
}

/*void MessageBox::OnButtonsUpdated()
{
	this->buttonsUpdated.disconnect(this);
	this->resetState();
}*/

void MessageBox::setProgress(f32 percent)
{
    progressImageColored.setSize(percent * 0.01f * progressImageBlack.getWidth(), progressImageColored.getHeight());
}

void MessageBox::setProgressBarInfo(const std::string & info)
{
	infoText.setText(info.c_str());
}

void MessageBox::UpdateButtons(GuiButton *button, const GuiController *controller, GuiTrigger *trigger)
{
    for(int i = 0; i < buttonCount; i++)
    {
		if(i == selectedButtonDPAD){
			messageButtons[i].messageButton->setState(STATE_SELECTED);
		}else{
			messageButtons[i].messageButton->clearState(STATE_SELECTED);
		}          
    }
}

void MessageBox::OnDPADClick(GuiButton *button, const GuiController *controller, GuiTrigger *trigger)
{
	if(trigger == &buttonATrigger)
	{
        //! do not auto launch when wiimote is pointing to screen and presses A
        if((controller->chan & (GuiTrigger::CHANNEL_2 | GuiTrigger::CHANNEL_3 | GuiTrigger::CHANNEL_4 | GuiTrigger::CHANNEL_5)) && controller->data.validPointer)
        {
            return;
        }
		OnOkButtonClick(button,controller,trigger);
		if(selectedButtonDPAD >= 0 && selectedButtonDPAD < buttonCount-1)
		{
			selectedButton = selectedButtonDPAD;
		}           
	}
	else if(trigger == &buttonBTrigger)
	{
		OnCancelButtonClick(button,controller,trigger);
	}
	else if(trigger == &buttonRightTrigger){
		selectedButtonDPAD++;
		if(selectedButtonDPAD >= buttonCount-1){
			selectedButtonDPAD = buttonCount-1;
		}
	}
	else if(trigger == &buttonLeftTrigger){
		selectedButtonDPAD--;
		if(selectedButtonDPAD < 0){
			selectedButtonDPAD = 0;
		}
	}
	UpdateButtons(button,controller,trigger);
}