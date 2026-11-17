#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include <Windows.h>
#include <Shlwapi.h>
#include <gdiplus.h>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shlwapi.lib")






namespace Utilities
{
	class File
	{
	public:
		static bool Exists(const std::wstring& path);

		static std::vector<BYTE> ReadAllBytes(const std::wstring& path);
		static std::wstring ReadAllText(const std::wstring& path);
		static std::vector<std::wstring> ReadAllLines(const std::wstring& path);
	};




	class Resource
	{
	public:
		static std::vector<BYTE> ReadAllBytes(int resourceId);
	};




	class Directory
	{
	public:
		static bool Exists(const std::wstring& path);
	};




	class Path
	{
	public:
		static std::wstring GetLauncherDirectory();
		static std::wstring GetDirectoryName(const std::wstring& path);

		static std::wstring Combine(const std::wstring& directory, const std::wstring& path);
	};




	class String
	{
	public:
		static std::wstring TrimLineBreaks(std::wstring string);
	};




	class Environment
	{
	public:
		static std::wstring GetVariable(const std::wstring& name);
		static void SetVariable(const std::wstring& name, const std::wstring& value);
	};




	class Registry
	{
	public:
		static bool TryGetUint32(const std::wstring& path, const std::wstring& keyName, uint32_t& value);
		static uint32_t GetUint32(const std::wstring& path, const std::wstring& keyName, uint32_t defaultValue);
		static bool SetUint32(const std::wstring& path, const std::wstring& keyName, uint32_t value);
	};




	class Wine
	{
	public:
		static bool IsRunning();
	};




	class Drawing
	{
	public:
		static Gdiplus::Rect GetZoomRectangle(Gdiplus::Image* image, const Gdiplus::Rect& bounds);
		static Gdiplus::Bitmap* ResizeImage(Gdiplus::Image* source, int width, int height);
		static void DrawImageUnscaled(Gdiplus::Graphics& graphics, Gdiplus::Image* image, int x, int y);

		static void FillRoundedRectangle(Gdiplus::Graphics& graphics, const Gdiplus::Brush* brush, const Gdiplus::RectF& bounds, float radius);
		static Gdiplus::Color GetControlColor();
	};
}
