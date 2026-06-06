#pragma once
#include <nlohmann/json.hpp>
#include <SDL3/SDL.h>
#include <fstream>
#include <iostream>
#include <filesystem>

using json = nlohmann::json;

struct CrosshairImage
{
	std::string name;
	SDL_FRect rect = { 0, 0, 0, 0 };
	// 相对锚点位置比例
	SDL_FPoint anchorPointK = { 0.5f, 0.5f };
	// 旋转锚点坐标
	SDL_FPoint anchorPoint = { 0.f, 0.f };
	float width;
	float height;
	//旋转角度
	float angle = 0.f;
	SDL_Texture* texture;
	// 透明度
	char alpha = 255;
	// 缩放
	float scale = 1.f;
	// 翻转模式
	int flip = 0;

	CrosshairImage(SDL_Renderer* renderer, std::string name)
	{
		this->name = name;
		std::string src = "./res/images/" + name;
		texture = IMG_LoadTexture(renderer, src.data());

		SDL_GetTextureSize(texture, &width, &height);

		rect.w = width;
		rect.h = height;

		setAnchorPoint();
	}
	~CrosshairImage()
	{
		SDL_DestroyTexture(texture);
	}
	// 重置大小
	void resetSize()
	{
		resize(1.0f);
	}
	// 设置缩放
	void resize(float scale)
	{
		// 缩放前锚点对应坐标
		SDL_FPoint lastPoint = { rect.x + rect.w * anchorPointK.x, rect.y + rect.h * anchorPointK.y };
		this->scale = scale;
		rect.w = width * scale;
		rect.h = height * scale;
		setPositionByAnchor(lastPoint);
	}
	// 设置旋转锚点坐标
	void setAnchorPoint()
	{
		anchorPoint.x = rect.w * anchorPointK.x;
		anchorPoint.y = rect.h * anchorPointK.y;
	}
	// 设置翻转模式（保持屏幕锚点不变）
	void setFlip(int newFlip)
	{
		if (this->flip == newFlip) return;

		// 记录当前锚点在屏幕上的位置
		SDL_FPoint screenAnchor = {
			rect.x + anchorPoint.x,
			rect.y + anchorPoint.y
		};

		// 先恢复到未翻转的锚点比例
		SDL_FPoint originalK;

		// 逆向当前翻转，得到原始比例
		originalK = anchorPointK;
		if (this->flip & SDL_FLIP_HORIZONTAL) {
			originalK.x = 1.0f - originalK.x;
		}
		if (this->flip & SDL_FLIP_VERTICAL) {
			originalK.y = 1.0f - originalK.y;
		}

		// 应用新翻转
		if (newFlip & SDL_FLIP_HORIZONTAL) {
			originalK.x = 1.0f - originalK.x;
		}
		if (newFlip & SDL_FLIP_VERTICAL) {
			originalK.y = 1.0f - originalK.y;
		}

		// 更新状态
		this->flip = newFlip;
		this->anchorPointK = originalK;

		// 重新计算锚点的本地坐标
		setAnchorPoint();

		// 将屏幕锚点位置保持不变
		setPositionByAnchor(screenAnchor);
	}
	// 设置相对位置
	void setPositionByAnchor(float x, float y)
	{
		rect.x = x - rect.w * anchorPointK.x;
		rect.y = y - rect.h * anchorPointK.y;
		setAnchorPoint();
	}

	void setPositionByAnchor(SDL_FPoint point)
	{
		rect.x = point.x - rect.w * anchorPointK.x;
		rect.y = point.y - rect.h * anchorPointK.y;
		setAnchorPoint();
	}
	// 获取相对位置（从屏幕坐标计算锚点比例）
	void getAnchor(float x, float y)
	{
		// 计算实际锚点比例（基于当前 rect）
		float rawX = (x - rect.x) / rect.w;
		float rawY = (y - rect.y) / rect.h;

		// 如果当前有翻转，需要反向计算原始锚点比例
		// 因为 anchorPointK 存储的是实际使用的比例（已应用翻转）
		if (flip & SDL_FLIP_HORIZONTAL) {
			anchorPointK.x = 1.0f - rawX;
		}
		else {
			anchorPointK.x = rawX;
		}

		if (flip & SDL_FLIP_VERTICAL) {
			anchorPointK.y = 1.0f - rawY;
		}
		else {
			anchorPointK.y = rawY;
		}

		setAnchorPoint();
	}

	void getAnchor(SDL_FPoint point)
	{
		getAnchor(point.x, point.y);
	}
	// 设置透明度
	void setAlpha(float a)
	{
		alpha = (char)(255 * a);
		SDL_SetTextureAlphaMod(texture, alpha);
	}
};

class JsonFile
{
private:
	json config;
	std::string src;
	CrosshairImage* crosshair;
public:
	
	JsonFile(std::string name, CrosshairImage* ch)
	{
		// 获取文件名
		std::filesystem::path path(name);
		name = path.stem().string();
		crosshair = ch;
		src = "./res/images/" + name + ".json";
		std::ifstream file(src);
		if (file.is_open())
		{
			file >> config;
			file.close();
			loadJsonToCrosshair();
		}
		else
		{
			save();
		}
	}
	// 保存
	void save()
	{
		config["rect"] = {
				   {"x", crosshair->rect.x},
				   {"y", crosshair->rect.y},
				   {"w", crosshair->rect.w},
				   {"h", crosshair->rect.h}
		};
		config["anchorPointK"] = {
			{"x", crosshair->anchorPointK.x},
			{"y", crosshair->anchorPointK.y}
		};
		config["anchorPoint"] = {
			{"x", crosshair->anchorPoint.x},
			{"y", crosshair->anchorPoint.y}
		};
		config["width"] = crosshair->width;
		config["height"] = crosshair->height;
		config["alpha"] = crosshair->alpha;
		config["scale"] = crosshair->scale;
		config["angle"] = crosshair->angle;
		config["flip"] = crosshair->flip;

		std::ofstream file(src);
		file << config.dump(4);
		file.close();
	}
	void loadJsonToCrosshair()
	{
		crosshair->rect.x = config["rect"]["x"];
		crosshair->rect.y = config["rect"]["y"];
		crosshair->rect.w = config["rect"]["w"];
		crosshair->rect.h = config["rect"]["h"];
		crosshair->anchorPointK.x = config["anchorPointK"]["x"];
		crosshair->anchorPointK.y = config["anchorPointK"]["y"];
		crosshair->anchorPoint.x = config["anchorPoint"]["x"];
		crosshair->anchorPoint.y = config["anchorPoint"]["y"];
		crosshair->flip = config["flip"];
		crosshair->width = config["width"];
		crosshair->height = config["height"];
		crosshair->angle = config["angle"];
		crosshair->setAlpha(config["alpha"] / 255.f);;
		crosshair->resize(config["scale"]);
	}
};

