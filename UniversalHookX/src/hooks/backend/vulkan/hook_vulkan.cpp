#include "../../../backend.hpp"
#include "../../../console/console.hpp"

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

#include "../../hooks.hpp"

static VkAllocationCallbacks*	g_Allocator = NULL;
static VkInstance               g_Instance = VK_NULL_HANDLE;
static VkPhysicalDevice         g_PhysicalDevice = VK_NULL_HANDLE;
static VkDevice                 g_Device = VK_NULL_HANDLE;
static uint32_t                 g_QueueFamily = (uint32_t)-1;
static VkPipelineCache          g_PipelineCache = VK_NULL_HANDLE;
static VkDescriptorPool         g_DescriptorPool = VK_NULL_HANDLE;
static uint32_t                 g_MinImageCount = 2;
static VkRenderPass				g_RenderPass = VK_NULL_HANDLE;
static ImGui_ImplVulkanH_Frame  g_Frames[16] = {};

static void check_vk_result(VkResult err) {
	if (err != VK_SUCCESS) {
		LOG("[!] [vulkan] Error: VkResult = %d\n", err);
	}
}

static bool SetupVulkan( ) {
	VkResult err;

	// Create Vulkan Instance
	{
		VkInstanceCreateInfo create_info = {};
		const char* extensions[ ] = { "VK_KHR_surface", "VK_KHR_win32_surface" };
		
		create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		create_info.enabledExtensionCount = RTL_NUMBER_OF(extensions);
		create_info.ppEnabledExtensionNames = extensions;

		// Create Vulkan Instance without any debug feature
		err = vkCreateInstance(&create_info, g_Allocator, &g_Instance);
		check_vk_result(err);

		LOG("[+] Vulkan: g_Instance: %p\n", g_Instance);
	}

	// Select GPU
	{
		uint32_t gpu_count;
		err = vkEnumeratePhysicalDevices(g_Instance, &gpu_count, NULL);
		check_vk_result(err);
		IM_ASSERT(gpu_count > 0);

		VkPhysicalDevice* gpus = new VkPhysicalDevice[sizeof(VkPhysicalDevice) * gpu_count];
		err = vkEnumeratePhysicalDevices(g_Instance, &gpu_count, gpus);
		check_vk_result(err);

		// If a number >1 of GPUs got reported, find discrete GPU if present, or use first one available. This covers
		// most common cases (multi-gpu/integrated+dedicated graphics). Handling more complicated setups (multiple
		// dedicated GPUs) is out of scope of this sample.
		int use_gpu = 0;
		for (int i = 0; i < (int)gpu_count; i++) {
			VkPhysicalDeviceProperties properties;
			vkGetPhysicalDeviceProperties(gpus[i], &properties);
			if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
				use_gpu = i;
				break;
			}
		}

		g_PhysicalDevice = gpus[use_gpu];
		LOG("[+] Vulkan: g_PhysicalDevice: %p\n", g_PhysicalDevice);

		delete[ ] gpus;
	}

	// Select graphics queue family
	{
		uint32_t count;
		vkGetPhysicalDeviceQueueFamilyProperties(g_PhysicalDevice, &count, NULL);
		VkQueueFamilyProperties* queues = new VkQueueFamilyProperties[sizeof(VkQueueFamilyProperties) * count];
		vkGetPhysicalDeviceQueueFamilyProperties(g_PhysicalDevice, &count, queues);
		for (uint32_t i = 0; i < count; i++)
			if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				g_QueueFamily = i;
				break;
			}
		delete[ ] queues;
		IM_ASSERT(g_QueueFamily != (uint32_t)-1);

		LOG("[+] Vulkan: g_QueueFamily: %u\n", g_QueueFamily);
	}

	// Create Logical Device (with 1 queue)
	{
		int device_extension_count = 1;
		const char* device_extensions[ ] = { "VK_KHR_swapchain" };
		const float queue_priority[ ] = { 1.0f };
		VkDeviceQueueCreateInfo queue_info[1] = {};
		queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_info[0].queueFamilyIndex = g_QueueFamily;
		queue_info[0].queueCount = 1;
		queue_info[0].pQueuePriorities = queue_priority;
		VkDeviceCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		create_info.queueCreateInfoCount = sizeof(queue_info) / sizeof(queue_info[0]);
		create_info.pQueueCreateInfos = queue_info;
		create_info.enabledExtensionCount = device_extension_count;
		create_info.ppEnabledExtensionNames = device_extensions;
		err = vkCreateDevice(g_PhysicalDevice, &create_info, g_Allocator, &g_Device);
		check_vk_result(err);

		LOG("[+] Vulkan: g_Device: %p\n", g_Device);
	}

	// Create Descriptor Pool
	{
		VkDescriptorPoolSize pool_sizes[ ] =
		{
			{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
		};
		VkDescriptorPoolCreateInfo pool_info = {};
		pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
		pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
		pool_info.pPoolSizes = pool_sizes;
		err = vkCreateDescriptorPool(g_Device, &pool_info, g_Allocator, &g_DescriptorPool);
		check_vk_result(err);

		LOG("[+] Vulkan: g_DescriptorPool: %llu\n", g_DescriptorPool);
	}

	return err == VK_SUCCESS;
}

static bool CreateImGuiRender(VkDevice device, VkSwapchainKHR swapchain) {
	VkResult err;
	uint32_t imageCount;

	err = vkGetSwapchainImagesKHR(device, swapchain, &imageCount, NULL);
	check_vk_result(err);
	VkImage backbuffers[16] = {};
	err = vkGetSwapchainImagesKHR(device, swapchain, &imageCount, backbuffers);
	check_vk_result(err);

	for (uint32_t i = 0; i < imageCount; i++)
		g_Frames[i].Backbuffer = backbuffers[i];

	for (uint32_t i = 0; i < imageCount; i++) {
		ImGui_ImplVulkanH_Frame* fd = &g_Frames[i];
		{
			VkCommandPoolCreateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
			info.queueFamilyIndex = g_QueueFamily;
			err = vkCreateCommandPool(device, &info, g_Allocator, &fd->CommandPool);
			check_vk_result(err);
		}
		{
			VkCommandBufferAllocateInfo info = {};
			info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			info.commandPool = fd->CommandPool;
			info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			info.commandBufferCount = 1;
			err = vkAllocateCommandBuffers(device, &info, &fd->CommandBuffer);
			check_vk_result(err);
		}
	}

	// Create the Render Pass
	{
		VkAttachmentDescription attachment = {};
		attachment.format = VK_FORMAT_B8G8R8A8_UNORM;
		attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		VkAttachmentReference color_attachment = {};
		color_attachment.attachment = 0;
		color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &color_attachment;
		VkSubpassDependency dependency = {};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		VkRenderPassCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		info.attachmentCount = 1;
		info.pAttachments = &attachment;
		info.subpassCount = 1;
		info.pSubpasses = &subpass;
		info.dependencyCount = 0;
		info.pDependencies = NULL;
		err = vkCreateRenderPass(device, &info, g_Allocator, &g_RenderPass);
		check_vk_result(err);
	}

	// Create The Image Views
	{
		VkImageViewCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		info.format = VK_FORMAT_B8G8R8A8_UNORM;
		info.components.r = VK_COMPONENT_SWIZZLE_R;
		info.components.g = VK_COMPONENT_SWIZZLE_G;
		info.components.b = VK_COMPONENT_SWIZZLE_B;
		info.components.a = VK_COMPONENT_SWIZZLE_A;
		VkImageSubresourceRange image_range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		info.subresourceRange = image_range;
		for (uint32_t i = 0; i < imageCount; i++) {
			ImGui_ImplVulkanH_Frame* fd = &g_Frames[i];
			info.image = fd->Backbuffer;
			err = vkCreateImageView(device, &info, g_Allocator, &fd->BackbufferView);
			check_vk_result(err);
		}
	}

	// Create Framebuffer
	{
		VkImageView attachment[1];
		VkFramebufferCreateInfo info = {};
		info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		info.renderPass = g_RenderPass;
		info.attachmentCount = 1;
		info.pAttachments = attachment;
		info.width = 1920;
		info.height = 1200;
		info.layers = 1;
		for (uint32_t i = 0; i < imageCount; i++) {
			ImGui_ImplVulkanH_Frame* fd = &g_Frames[i];
			attachment[0] = fd->BackbufferView;
			err = vkCreateFramebuffer(device, &info, g_Allocator, &fd->Framebuffer);
			check_vk_result(err);
		}
	}

	return true;
}

static std::add_pointer_t<VkResult VKAPI_CALL(VkDevice, VkSwapchainKHR, uint64_t, VkSemaphore, VkFence, uint32_t*)> oAcquireNextImageKHR;
static VkResult VKAPI_CALL hkAcquireNextImageKHR(VkDevice device,
												 VkSwapchainKHR swapchain,
												 uint64_t timeout,
												 VkSemaphore semaphore,
												 VkFence fence,
												 uint32_t* pImageIndex) {
	VkResult err = oAcquireNextImageKHR(device, swapchain, timeout, semaphore, fence, pImageIndex);

	static bool _ = CreateImGuiRender(device, swapchain);
	g_Device = device;

	return err;
}

static std::add_pointer_t<VkResult VKAPI_CALL(VkQueue, const VkPresentInfoKHR*)> oQueuePresentKHR;
static VkResult VKAPI_CALL hkQueuePresentKHR(VkQueue queue,
											 const VkPresentInfoKHR* pPresentInfo) {
	static bool bLockDrawing;
	if (!bLockDrawing && g_Device && H::bShowDemoWindow && !H::bShuttingDown) {
		bLockDrawing = true;

		for (uint32_t i = 0; i < pPresentInfo->swapchainCount; i++) {
			VkSwapchainKHR swapchain = pPresentInfo->pSwapchains[i];

			VkResult err;
			ImGui_ImplVulkanH_Frame* fd = &g_Frames[pPresentInfo->pImageIndices[i]];
			{
				err = vkResetCommandPool(g_Device, fd->CommandPool, 0);
				vkResetCommandBuffer(fd->CommandBuffer, 0);
				check_vk_result(err);
				VkCommandBufferBeginInfo info = {};
				info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
				info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
				err = vkBeginCommandBuffer(fd->CommandBuffer, &info);
				check_vk_result(err);
			}
			{
				VkRenderPassBeginInfo info = {};
				info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
				info.renderPass = g_RenderPass;
				info.framebuffer = fd->Framebuffer;
				info.renderArea.extent.width = 1920;
				info.renderArea.extent.height = 1200;

				VkClearValue clearColor = { {{1.0f, 1.0f, 1.0f, 1.0f}} };
				info.clearValueCount = 1;
				info.pClearValues = &clearColor;
				vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
			}

			if (!ImGui::GetIO().BackendRendererUserData) {
				ImGui_ImplVulkan_InitInfo init_info = {};
				init_info.Instance = g_Instance;
				init_info.PhysicalDevice = g_PhysicalDevice;
				init_info.Device = g_Device;
				init_info.QueueFamily = g_QueueFamily;
				init_info.Queue = queue;
				init_info.PipelineCache = g_PipelineCache;
				init_info.DescriptorPool = g_DescriptorPool;
				init_info.Subpass = 0;
				init_info.MinImageCount = g_MinImageCount;
				init_info.ImageCount = g_MinImageCount;
				init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
				init_info.Allocator = g_Allocator;
				init_info.CheckVkResultFn = check_vk_result;
				ImGui_ImplVulkan_Init(&init_info, g_RenderPass);

				ImGui_ImplVulkan_CreateFontsTexture(fd->CommandBuffer);
				ImGui_ImplVulkan_DestroyFontUploadObjects( );
			}

			ImGui_ImplVulkan_NewFrame();
			ImGui_ImplWin32_NewFrame( );
			ImGui::NewFrame( );

			if (H::bShowDemoWindow) {
				ImGui::ShowDemoWindow( );
			}

			ImGui::Render( );

			// Record dear imgui primitives into command buffer
			ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData( ), fd->CommandBuffer);

			// Submit command buffer
			vkCmdEndRenderPass(fd->CommandBuffer);
			{
				VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
				VkSubmitInfo info = {};
				info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
				info.waitSemaphoreCount = 0;
				info.pWaitSemaphores = NULL;
				info.pWaitDstStageMask = &wait_stage;
				info.commandBufferCount = 1;
				info.pCommandBuffers = &fd->CommandBuffer;
				info.signalSemaphoreCount = 0;
				info.pSignalSemaphores = NULL;

				err = vkEndCommandBuffer(fd->CommandBuffer);
				check_vk_result(err);
				err = vkQueueSubmit(queue, 1, &info, NULL);
				check_vk_result(err);
			}
		}
		bLockDrawing = false;
	}

	return oQueuePresentKHR(queue, pPresentInfo);
}

static std::add_pointer_t<VkResult VKAPI_CALL(VkDevice, const VkSwapchainCreateInfoKHR*, const VkAllocationCallbacks*, VkSwapchainKHR*)> oCreateSwapchainKHR;
static VkResult VKAPI_CALL hkCreateSwapchainKHR(VkDevice device,
												const VkSwapchainCreateInfoKHR* pCreateInfo,
												const VkAllocationCallbacks* pAllocator,
												VkSwapchainKHR* pSwapchain) {
	VkResult err = oCreateSwapchainKHR(device, pCreateInfo, pAllocator, pSwapchain);

	CreateImGuiRender(device, *pSwapchain);

	return err;
}

namespace VK {
	void Hook(HWND hwnd) {
		HMODULE vulkan1 = GetModuleHandleA("vulkan-1.dll");
		if (vulkan1) {
			LOG("[+] Vulkan: ImageBase: 0x%p\n", vulkan1);
			
			if (!SetupVulkan( )) {
				LOG("[!] SetupVulkan() failed.\n");
				return;
			}

			void* fnAcquireNextImageKHR = reinterpret_cast<void*>(vkGetDeviceProcAddr(g_Device, "vkAcquireNextImageKHR"));
			void* fnQueuePresentKHR = reinterpret_cast<void*>(vkGetDeviceProcAddr(g_Device, "vkQueuePresentKHR"));
			void* fnCreateSwapchainKHR = reinterpret_cast<void*>(vkGetDeviceProcAddr(g_Device, "vkCreateSwapchainKHR"));

			g_Device = NULL;

			if (fnAcquireNextImageKHR) {
				// Init ImGui
				ImGui::CreateContext( );
				ImGui_ImplWin32_Init(hwnd);

				ImGuiIO& io = ImGui::GetIO( );

				io.IniFilename = nullptr;
				io.LogFilename = nullptr;

				// Hook
				LOG("[+] Vulkan: fnAcquireNextImageKHR: 0x%p\n", fnAcquireNextImageKHR);
				LOG("[+] Vulkan: fnQueuePresentKHR: 0x%p\n", fnQueuePresentKHR);
				LOG("[+] Vulkan: fnCreateSwapchainKHR: 0x%p\n", fnCreateSwapchainKHR);

				static MH_STATUS aniStatus = MH_CreateHook(reinterpret_cast<void**>(fnAcquireNextImageKHR), &hkAcquireNextImageKHR, reinterpret_cast<void**>(&oAcquireNextImageKHR));
				static MH_STATUS qpStatus = MH_CreateHook(reinterpret_cast<void**>(fnQueuePresentKHR), &hkQueuePresentKHR, reinterpret_cast<void**>(&oQueuePresentKHR));
				static MH_STATUS csStatus = MH_CreateHook(reinterpret_cast<void**>(fnCreateSwapchainKHR), &hkCreateSwapchainKHR, reinterpret_cast<void**>(&oCreateSwapchainKHR));

				MH_EnableHook(fnAcquireNextImageKHR);
				MH_EnableHook(fnQueuePresentKHR);
				MH_EnableHook(fnCreateSwapchainKHR);
			}
		}
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
	void Hook(HWND hwnd) { LOG("[!] Vulkan backend is not enabled!\n"); }
	void Unhook( ) { }
}
#endif
