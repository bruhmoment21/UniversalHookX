#include "../../../backend.hpp"

#ifdef BACKEND_ENABLE_VULKAN
#include <Windows.h>

#include <unordered_map>
#include <memory>
#include <mutex>

// https://vulkan.lunarg.com/
#include <vulkan/vulkan.h>
#pragma comment(lib, "vulkan-1.lib")

#include "hook_vulkan.hpp"

#include "../../../dependencies/imgui/imgui_impl_vulkan.h"
#include "../../../dependencies/imgui/imgui_impl_win32.h"
#include "../../../dependencies/minhook/MinHook.h"

#include "../../../console/console.hpp"
#include "../../hooks.hpp"

static std::add_pointer_t<VkResult VKAPI_CALL(VkCommandBuffer, const VkCommandBufferBeginInfo*)> oBeginCommandBuffer;
static VkResult VKAPI_CALL hkBeginCommandBuffer(VkCommandBuffer commandBuffer,
												const VkCommandBufferBeginInfo* pBeginInfo) {
	return oBeginCommandBuffer(commandBuffer, pBeginInfo);
}

static std::add_pointer_t<VkResult VKAPI_CALL(VkQueue, const VkPresentInfoKHR*)> oQueuePresentKHR;
static VkResult VKAPI_CALL hkQueuePresentKHR(VkQueue queue,
											 const VkPresentInfoKHR* pPresentInfo) {
	return oQueuePresentKHR(queue, pPresentInfo);
}

static std::add_pointer_t<VkResult VKAPI_CALL(VkPhysicalDevice, const VkDeviceCreateInfo*, const VkAllocationCallbacks*, VkDevice*)> oCreateDevice;
static VkResult VKAPI_CALL hkCreateDevice(VkPhysicalDevice physicalDevice,
										  const VkDeviceCreateInfo* pCreateInfo,
										  const VkAllocationCallbacks* pAllocator,
										  VkDevice* pDevice) {
	return oCreateDevice(physicalDevice, pCreateInfo, pAllocator, pDevice);
}

static std::add_pointer_t<VkResult VKAPI_CALL(VkDevice, const VkSwapchainCreateInfoKHR*, const VkAllocationCallbacks*, VkSwapchainKHR*)> oCreateSwapchainKHR;
static VkResult VKAPI_CALL hkCreateSwapchainKHR(VkDevice device,
												const VkSwapchainCreateInfoKHR* pCreateInfo,
												const VkAllocationCallbacks* pAllocator,
												VkSwapchainKHR* pSwapchain) {
	return oCreateSwapchainKHR(device, pCreateInfo, pAllocator, pSwapchain);
}

static std::add_pointer_t<PFN_vkVoidFunction VKAPI_CALL(VkInstance, const char*)> oGetInstanceProcAddr;
static PFN_vkVoidFunction VKAPI_CALL hkGetInstanceProcAddr(VkInstance instance,
														   const char* pName) {
	return oGetInstanceProcAddr(instance, pName);
}

namespace VK {
	void Hook(HWND hwnd) {
		LOG("[!] Vulkan not implemented yet!\n");
	}

	void Unhook( ) {
		if (ImGui::GetCurrentContext( )) {
			if (ImGui::GetIO( ).BackendRendererUserData)
				ImGui_ImplVulkan_Shutdown( );

			ImGui_ImplWin32_Shutdown( );
			ImGui::DestroyContext( );
		}
	}
}
#else
#include <Windows.h>
namespace VK {
	void Hook(HWND hwnd) { }
	void Unhook( ) { }
}
#endif
