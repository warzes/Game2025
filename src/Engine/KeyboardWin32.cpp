#include "stdafx.h"
#if PLATFORM_WINDOWS
#include "Keyboard.h"
//=============================================================================
namespace
{
	void KeyDown(int key, KeyState& state) noexcept
	{
		if (key < 0 || key > 0xfe)
			return;

		auto ptr = reinterpret_cast<uint32_t*>(&state);

		const unsigned int bf = 1u << (key & 0x1f);
		ptr[(key >> 5)] |= bf;
	}

	void KeyUp(int key, KeyState& state) noexcept
	{
		if (key < 0 || key > 0xfe)
			return;

		auto ptr = reinterpret_cast<uint32_t*>(&state);

		const unsigned int bf = 1u << (key & 0x1f);
		ptr[(key >> 5)] &= ~bf;
	}
}
//=============================================================================
void KeyboardStateTracker::Update(const KeyState& state) noexcept
{
	auto currPtr = reinterpret_cast<const uint32_t*>(&state);
	auto prevPtr = reinterpret_cast<const uint32_t*>(&lastState);
	auto releasedPtr = reinterpret_cast<uint32_t*>(&released);
	auto pressedPtr = reinterpret_cast<uint32_t*>(&pressed);
	for (size_t j = 0; j < (256 / 32); ++j)
	{
		*pressedPtr = *currPtr & ~(*prevPtr);
		*releasedPtr = ~(*currPtr) & *prevPtr;

		++currPtr;
		++prevPtr;
		++releasedPtr;
		++pressedPtr;
	}

	lastState = state;
}
//=============================================================================
void KeyboardStateTracker::Reset() noexcept
{
	memset(this, 0, sizeof(KeyboardStateTracker));
}
//=============================================================================
bool Keyboard::Create()
{
	Reset();
	return true;
}
//=============================================================================
void Keyboard::Destroy()
{
}
//=============================================================================
void Keyboard::Reset()
{
	memset(&m_state, 0, sizeof(KeyState));
}
//=============================================================================
void Keyboard::processMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
	bool down = false;

	switch (message)
	{
	case WM_ACTIVATE:
	case WM_ACTIVATEAPP:
		Reset();
		return;

	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		down = true;
		break;

	case WM_KEYUP:
	case WM_SYSKEYUP:
		break;

	default:
		return;
	}

	int vk = LOWORD(wParam);
	// We want to distinguish left and right shift/ctrl/alt keys
	switch (vk)
	{
	case VK_SHIFT:
	case VK_CONTROL:
	case VK_MENU:
	{
		if (vk == VK_SHIFT && !down)
		{
			// Workaround to ensure left vs. right shift get cleared when both were pressed at same time
			KeyUp(VK_LSHIFT, m_state);
			KeyUp(VK_RSHIFT, m_state);
		}

		bool isExtendedKey = (HIWORD(lParam) & KF_EXTENDED) == KF_EXTENDED;
		int scanCode = LOBYTE(HIWORD(lParam)) | (isExtendedKey ? 0xe000 : 0);
		vk = LOWORD(MapVirtualKeyW(static_cast<UINT>(scanCode), MAPVK_VSC_TO_VK_EX));
	}
	break;

	default:
		break;
	}

	if (down)
	{
		KeyDown(vk, m_state);
	}
	else
	{
		KeyUp(vk, m_state);
	}
}
//=============================================================================
#endif // PLATFORM_WINDOWS