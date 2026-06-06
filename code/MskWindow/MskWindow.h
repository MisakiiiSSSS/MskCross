#pragma once
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3/SDL_keyboard.h>
#include <Windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include "../Const/Color.h"
#include <nlohmann/json.hpp>
#include "../Json/LoadJson.h"


void MskWindowInit()
{
	SetConsoleOutputCP(65001);
	SDL_Init(SDL_INIT_VIDEO);
}

class MskWindow
{
private:
	SDL_Window* window = nullptr;
	SDL_Renderer* renderer = nullptr;
	int windowWidth = 1920;
	int windowHeight = 1080;
	const char* windowTitle = "Misaki's cross";
	// 屏幕信息
	int screenWidth = 1920;
	int screenHeight = 1080;
	SDL_DisplayID displayID;
	SDL_Rect screenRect;
	SDL_FPoint screenCenter;
	// 主循环
	bool isRunning = true;
	// 准星结构
	CrosshairImage* crosshair = nullptr;
	// config文件
	JsonFile* config = nullptr;
	// 显示屏幕中心
	bool isShowScreenCenter = false;
	json imgJson;

	// 多线程命令处理
	std::thread inputThread;
	std::atomic<bool> inputThreadRunning = false;

	std::mutex commandQueueMutex;
	std::queue<std::string> commandQueue;

	// 透明且可穿透
	void setWindowClickThrough()
	{
		SDL_PropertiesID props = SDL_GetWindowProperties(window);
		HWND hwnd = (HWND)SDL_GetPointerProperty(
			props,
			SDL_PROP_WINDOW_WIN32_HWND_POINTER,
			NULL
		);
		SetWindowLongPtr(hwnd, GWL_EXSTYLE,
			GetWindowLongPtr(hwnd, GWL_EXSTYLE) | WS_EX_TRANSPARENT | WS_EX_LAYERED);
	}

	void getScreenSize()
	{
		displayID = SDL_GetPrimaryDisplay();
		SDL_GetDisplayUsableBounds(displayID, &screenRect);

		windowWidth = screenRect.w;
		windowHeight = screenRect.h;
		screenCenter = { screenWidth / 2.f, screenHeight / 2.f };
	}
	// 输入循环
	void inputLoop()
	{
		while (inputThreadRunning)
		{
			std::string command;
			std::getline(std::cin, command);
			if (!command.empty())
			{
				std::lock_guard<std::mutex> lock(commandQueueMutex);
				commandQueue.push(command);
			}
		}
	}

	// 从队列里取命令
	bool fetchCommand(std::string& outCommand)
	{
		std::lock_guard<std::mutex> lock(commandQueueMutex);
		if (!commandQueue.empty())
		{
			outCommand = commandQueue.front();
			commandQueue.pop();
			return true;
		}
		return false;
	}
public:

	MskWindow()
	{
		getScreenSize();
		window = SDL_CreateWindow(windowTitle, windowWidth, windowHeight, SDL_WINDOW_TRANSPARENT  | SDL_WINDOW_ALWAYS_ON_TOP | SDL_WINDOW_BORDERLESS);
		renderer = SDL_CreateRenderer(window, NULL);
		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
		setWindowClickThrough();
		// 加载图标
		SDL_Surface* iconSurface = IMG_Load("./res/icon.bmp");
		SDL_SetWindowIcon(window, iconSurface);
		SDL_DestroySurface(iconSurface);
		// 打开配置文件
		std::ifstream file("./res/config.json");
		if (file.is_open())
		{
			file >> imgJson;
			file.close();
			isShowScreenCenter = imgJson["showCross"];
			openHandle(imgJson["name"]);
		}
		// 如果没有配置文件则新建一个
		else
		{
			imgJson.clear();
			imgJson["name"] = "null";
			imgJson["showCross"] = false;
			std::ofstream file("./res/config.json");
			file << imgJson.dump(4);
			file.close();
		}
	}

	~MskWindow()
	{
		SDL_DestroyRenderer(renderer);
		SDL_DestroyWindow(window);
		if (config != nullptr)
		{
			config->save();
			delete config;
		}
		if (crosshair != nullptr)
		{
			// 保存配置
			imgJson["name"] = crosshair->name;
			imgJson["showCross"] = isShowScreenCenter;
			std::ofstream file("./res/config.json");
			file << imgJson.dump(4);
			file.close();
			delete crosshair;
		}
	}

	// 主循环
	void mainLoop()
	{
		showHelp();
		SDL_Event event{};
		// 如果无法打开图片应打开默认图片
		if (crosshair == nullptr)
		{
			openHandle("default.png");
		}
		// 预先刷新窗口显示内容
		refreshRender();
		commandTitle();

		// 启动输入线程
		inputThreadRunning = true;
		inputThread = std::thread(&MskWindow::inputLoop, this);

		// 主循环
		while (isRunning)
		{
			while (SDL_PollEvent(&event))
			{
				if (event.type == SDL_EVENT_QUIT)
				{
					isRunning = false;
				}
			}
			// 检测输入线程是否有命令输入
			std::string cmd;
			while (fetchCommand(cmd))
			{
				commandHandle(cmd);
			}


			SDL_Delay(16);
			
		}

		inputThreadRunning = false;
		if (inputThread.joinable())
		{
			inputThread.join();
		}
	}
private:
	// 处理输入命令
	void commandHandle(const std::string& command)
	{
		// 读取命令并分割
		std::vector<std::string> tokens;
		std::stringstream ss(command);
		std::string token;
		while (ss >> token)
		{
			tokens.push_back(token);
		}
		if (tokens.empty())
		{
			return;
		}
		// 处理判断命令
		if (tokens[0] == "exit")
		{
			isRunning = false;
			return;
		}
		else if (tokens[0] == "save")
		{
			config->save();
		}
		else if (tokens[0] == "help")
		{
			showHelp();
		}
		else if (tokens[0] == "open" && tokens.size() == 2)
		{
			openHandle(tokens[1]);
		}
		else if (tokens[0] == "move")
		{
			if (tokens.size() == 1)
			{
				repositionHandle();
			}
			else if (tokens.size() == 2)
			{
				
				if (tokens[1] == "mouse" )
				{
					moveToMouse();
				}
				else if (tokens[1] == "center")
				{
					moveToCenter();
				}
				else
				{
					errorCommand();
				}
			}
			else
			{
				errorCommand();
			}
		}
		else if (tokens[0] == "cross")
		{
			if (tokens.size() == 2)
			{
				if (tokens[1] == "true")
				{
					isShowScreenCenter = true;
					refreshRender();
				}
				else if (tokens[1] == "false")
				{
					isShowScreenCenter = false;
					refreshRender();
				}
				else
				{
					errorCommand();
				}
			}
			else
			{
				errorCommand();
			}
		}
		else if (tokens[0] == "set")
		{
			if (tokens.size() == 2)
			{
				if (tokens[1] == "anchor")
				{
					setAnchorPosition();
				}
				else
				{
					errorCommand();
				}
			}
			else if (tokens.size() == 3)
			{
				if (tokens[1] == "scale")
				{
					if (isDecimal(tokens[2]))
					{
						float scale = std::stof(tokens[2]);
						if (scale < 0.01f || scale > 100.f)
						{
							SDL_Log("MskCross >> Range of scale is 0.01 to 100.");
						}
						else
						{
							crosshair->resize(scale);
							refreshRender();
						}
					}
					else
					{
						errorCommand();
					}
				}
				else if (tokens[1] == "alpha")
				{
					if (isDecimal(tokens[2]))
					{
						float alpha = std::stof(tokens[2]);
						if (alpha < 0 || alpha > 1)
						{
							SDL_Log("MskCross >> Range of alpha is 0 to 1.");
						}
						else
						{
							crosshair->setAlpha(alpha);
							refreshRender();
						}
					}
					else
					{
						errorCommand();
					}
				}
				else if (tokens[1] == "angle")
				{
					if (isDecimal(tokens[2]))
					{
						float angle = std::stof(tokens[2]);
						if (angle < 0.f || angle > 360.f)
						{
							SDL_Log("MskCross >> Range of angle is 0 to 360.");
						}
						else
						{
							crosshair->angle = angle;
							refreshRender();
						}
					}
					else
					{
						errorCommand();
					}
				}
				else if (tokens[1] == "flip")
				{
					if (tokens[2] == "n")
					{
						crosshair->setFlip(0);
						refreshRender();
					}
					else if (tokens[2] == "h")
					{
						crosshair->setFlip(1);
						refreshRender();
					}
					else if (tokens[2] == "v")
					{
						crosshair->setFlip(2);
						refreshRender();
					}
					else if (tokens[2] == "hv")
					{
						crosshair->setFlip(3);
						refreshRender();
					}
				}
				else
				{
					errorCommand();
				}
			}
			else
			{
				errorCommand();
			}
		}
		else
		{
			errorCommand();
		}
		commandTitle();
	}
	// 打开新图片
	void openHandle(std::string name)
	{
		if ((hasSuffix(name, ".png") || hasSuffix(name, ".jpg")) && std::filesystem::exists("./res/images/" + name))
		{
			if (config != nullptr)
			{
				config->save();
				delete config;
			}
			if (crosshair != nullptr)
			{
				delete crosshair;
			}
			crosshair = new CrosshairImage(renderer, name);
			config = new JsonFile(name, crosshair);
			refreshRender();
		}
		else
		{
			SDL_Log("MskCross >> Cannot find file or file is not png or jpg.");
		}
	}
	// 处理位置修改
	void repositionHandle()
	{
		bool isReposition = true;
		SDL_RaiseWindow(window);
		SDL_Event event{};
		while (isReposition)
		{
			while (SDL_PollEvent(&event))
			{
				switch (event.type)
				{
				case SDL_EVENT_WINDOW_FOCUS_LOST:
					isReposition = false;
					break;
				case SDL_EVENT_KEY_DOWN:
					switch (event.key.key)
					{
					case SDLK_W:
						if (crosshair->rect.y > 1)
						{
							crosshair->rect.y -= 1;
							refreshRender();
						}
						break;
					case SDLK_A:
						if (crosshair->rect.x > 1)
						{
							crosshair->rect.x -= 1;
							refreshRender();
						}
						break;
					case SDLK_S:
						if (crosshair->rect.y < windowHeight)
						{
							crosshair->rect.y += 1;
							refreshRender();
						}
						break;
					case SDLK_D:
						if (crosshair->rect.x < windowWidth)
						{
							crosshair->rect.x += 1;
							refreshRender();
						}
						break;
					case SDLK_SPACE:
						isReposition = false;
						break;
					}
					break;
				}
				
			}

			SDL_Delay(5);
		}
	}
	// 移动到鼠标位置
	void moveToMouse() 
	{
		POINT cursorPos;
		GetCursorPos(&cursorPos);
		crosshair->setPositionByAnchor((float)cursorPos.x, (float)cursorPos.y);
		refreshRender();
	}
	// 移动准星到屏幕中央
	void moveToCenter()
	{
		crosshair->setPositionByAnchor(screenCenter);
		refreshRender();
	}
	// 设置图片鼠标相对位置
	void setAnchorPosition()
	{
		POINT cursorPos;
		GetCursorPos(&cursorPos);
		crosshair->getAnchor((float)cursorPos.x, (float)cursorPos.y);
	}
	// 显示输入命令开头内容
	void commandTitle()
	{
		std::cout << "Command  >> ";
	}
	// 错误命令输出
	void errorCommand()
	{
		SDL_Log("MskCross >> Unknown command. Please enter [help] to get all available commands.");
	}
	// 刷新显示内容
	void refreshRender()
	{
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
		SDL_RenderClear(renderer);

		// 显示
		SDL_RenderTextureRotated(renderer, crosshair->texture, NULL, &(crosshair->rect), crosshair->angle, &(crosshair->anchorPoint), static_cast<SDL_FlipMode>(crosshair->flip));
		// 显示屏幕中心十字
		if (isShowScreenCenter)
		{
			drawCross();
		}

		SDL_RenderPresent(renderer);
	}
	// 绘制屏幕中心十字
	void drawCross()
	{
		float length = 20.f;
		SDL_SetRenderDrawColor(renderer, HotPink.r, HotPink.g, HotPink.b, HotPink.a);
		SDL_RenderLine(renderer, screenCenter.x, screenCenter.y - length, screenCenter.x, screenCenter.y + length);
		SDL_RenderLine(renderer, screenCenter.x - length, screenCenter.y, screenCenter.x + length, screenCenter.y);
	}
	// 判断是否是小数
	bool isDecimal(const std::string& str) {
		if (str.empty()) return false;

		std::istringstream iss(str);
		double value;

		// 尝试读取一个浮点数
		iss >> value;

		// 检查是否成功读取且没有剩余字符
		return !iss.fail() && iss.eof();
	}
	// 判断是否是整数
	bool isInteger(const std::string& str) {
		if (str.empty()) return false;

		std::istringstream iss(str);
		int value;
		iss >> value;

		// 检查是否成功读取且没有剩余字符
		return !iss.fail() && iss.eof();
	}
	// 判断是否是指定类型文件
	bool hasSuffix(const std::string& str, const std::string& suffix) {
		if (str.length() < suffix.length()) return false;
		return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
	}
	// 显示全部命令
	void showHelp()
	{
		std::ifstream file("./res/help.txt");
		if (file.is_open())
		{
			std::string line;
			while (std::getline(file, line))
			{
				std::cout << line << std::endl;
			}
			file.close();
		}
	}
};
