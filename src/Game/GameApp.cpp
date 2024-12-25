#include "stdafx.h"

void GameApp()
{
	EngineAppCreateInfo engineAppCreateInfo{};
	EngineApp engine;
	if (engine.Create(engineAppCreateInfo))
	{
		auto& rhi = engine.GetRenderSystem();
		auto graphicsContext = rhi.GetGraphicsContext();

		struct MeshVertex
		{
			glm::vec3 position;
			glm::vec2 uv;
			glm::vec3 normal;
		};

		struct MeshConstants
		{
			glm::mat4 worldMatrix;
			uint32_t vertexBufferIndex;
			uint32_t textureIndex;
		};

		struct MeshPassConstants
		{
			glm::mat4 viewMatrix;
			glm::mat4 projectionMatrix;
			glm::vec3 cameraPosition;
		};

		std::unique_ptr<TextureResource> mDepthBuffer;
		std::unique_ptr<TextureResource> mWoodTexture;
		std::unique_ptr<BufferResource> mMeshVertexBuffer;
		std::array<std::unique_ptr<BufferResource>, NUM_FRAMES_IN_FLIGHT> mMeshConstantBuffers;
		std::unique_ptr<BufferResource> mMeshPassConstantBuffer;
		PipelineResourceSpace mMeshPerObjectResourceSpace;
		PipelineResourceSpace mMeshPerPassResourceSpace;
		std::unique_ptr<Shader> mMeshVertexShader;
		std::unique_ptr<Shader> mMeshPixelShader;
		std::unique_ptr<PipelineStateObject> mMeshPSO;

		// InitializeMeshResources
{
			MeshVertex meshVertices[36] = {
				{{1.0f, -1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
				{{1.0f, -1.0f, -1.0f}, {1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
				{{-1.0f, -1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
				{{-1.0f, -1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
				{{1.0f, -1.0f, -1.0f}, {1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
				{{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f}, {0.0f, -1.0f, 0.0f}},
				{{1.0f, 1.0f, -1.0f}, {1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
				{{1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
				{{-1.0f, 1.0f, -1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
				{{-1.0f, 1.0f, -1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
				{{1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
				{{-1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
				{{-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
				{{-1.0f, 1.0f, -1.0f}, {1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
				{{-1.0f, -1.0f, 1.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
				{{-1.0f, -1.0f, 1.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
				{{-1.0f, 1.0f, -1.0f}, {1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
				{{-1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
				{{-1.0f, -1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
				{{-1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
				{{1.0f, -1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
				{{1.0f, -1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
				{{-1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
				{{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
				{{1.0f, -1.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
				{{1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
				{{1.0f, -1.0f, -1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
				{{1.0f, -1.0f, -1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
				{{1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
				{{1.0f, 1.0f, -1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
				{{1.0f, -1.0f, -1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
				{{1.0f, 1.0f, -1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
				{{-1.0f, -1.0f, -1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
				{{-1.0f, -1.0f, -1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
				{{1.0f, 1.0f, -1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
				{{-1.0f, 1.0f, -1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
			};

			BufferCreationDesc meshVertexBufferDesc{};
			meshVertexBufferDesc.size = sizeof(meshVertices);
			meshVertexBufferDesc.accessFlags = BufferAccessFlags::gpuOnly;
			meshVertexBufferDesc.viewFlags = BufferViewFlags::srv;
			meshVertexBufferDesc.stride = sizeof(MeshVertex);
			meshVertexBufferDesc.isRawAccess = true;

			mMeshVertexBuffer = CreateBuffer(meshVertexBufferDesc);

			auto bufferUpload = std::make_unique<BufferUpload>();
			bufferUpload->buffer = mMeshVertexBuffer.get();
			bufferUpload->bufferData = std::make_unique<uint8_t[]>(sizeof(meshVertices));
			bufferUpload->bufferDataSize = sizeof(meshVertices);

			memcpy_s(bufferUpload->bufferData.get(), sizeof(meshVertices), meshVertices, sizeof(meshVertices));

			gRHI.GetUploadContextForCurrentFrame().AddBufferUpload(std::move(bufferUpload)); // добавить в очередь - загрузка данных сразу на gpu, эффективней чем хранить в cpu (в пред примере с треугольником)

			mWoodTexture = CreateTextureFromFile("Data/Textures/Wood.dds");

			MeshConstants meshConstants;
			meshConstants.vertexBufferIndex = mMeshVertexBuffer->descriptorHeapIndex;
			meshConstants.textureIndex = mWoodTexture->descriptorHeapIndex;

			BufferCreationDesc meshConstantDesc{};
			meshConstantDesc.size = sizeof(MeshConstants);
			meshConstantDesc.accessFlags = BufferAccessFlags::hostWritable;
			meshConstantDesc.viewFlags = BufferViewFlags::cbv;

			for (uint32_t frameIndex = 0; frameIndex < NUM_FRAMES_IN_FLIGHT; frameIndex++) // для каждого кадра свой буфер чтобы не было ситуации когда идет запись в буфер, хотя с ним идет работа на видеокарте
			{
				mMeshConstantBuffers[frameIndex] = CreateBuffer(meshConstantDesc);
				mMeshConstantBuffers[frameIndex]->SetMappedData(&meshConstants, sizeof(MeshConstants));
			}

			BufferCreationDesc meshPassConstantDesc{}; // так как ставится один раз - не нужно создавать копии для кадров
			meshPassConstantDesc.size = sizeof(MeshPassConstants);
			meshPassConstantDesc.accessFlags = BufferAccessFlags::hostWritable;
			meshPassConstantDesc.viewFlags = BufferViewFlags::cbv;

			auto screenSize = rhi.GetFrameBufferSize();

			float fieldOfView = 3.14159f / 4.0f;
			float aspectRatio = (float)screenSize.x / (float)screenSize.y;
			glm::vec3 cameraPosition = glm::vec3(-3.0f, 3.0f, -8.0f);

			MeshPassConstants passConstants;
			passConstants.viewMatrix = glm::lookAt(cameraPosition, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
			passConstants.projectionMatrix = glm::perspective(fieldOfView, aspectRatio, 0.001f, 1000.0f);
			passConstants.cameraPosition = cameraPosition;

			mMeshPassConstantBuffer = CreateBuffer(meshPassConstantDesc);
			mMeshPassConstantBuffer->SetMappedData(&passConstants, sizeof(MeshPassConstants));

			TextureCreationDesc depthBufferDesc;
			depthBufferDesc.resourceDesc.Format = DXGI_FORMAT_D32_FLOAT;
			depthBufferDesc.resourceDesc.Width = screenSize.x;
			depthBufferDesc.resourceDesc.Height = screenSize.y;
			depthBufferDesc.viewFlags = TextureViewFlags::srv | TextureViewFlags::dsv;

			mDepthBuffer = CreateTexture(depthBufferDesc);

			ShaderCreationDesc meshShaderVSDesc;
			meshShaderVSDesc.shaderName = L"Mesh.hlsl";
			meshShaderVSDesc.entryPoint = L"VertexShader";
			meshShaderVSDesc.type = ShaderType::vertex;

			ShaderCreationDesc meshShaderPSDesc;
			meshShaderPSDesc.shaderName = L"Mesh.hlsl";
			meshShaderPSDesc.entryPoint = L"PixelShader";
			meshShaderPSDesc.type = ShaderType::pixel;

			mMeshVertexShader = CreateShader(meshShaderVSDesc);
			mMeshPixelShader = CreateShader(meshShaderPSDesc);

			GraphicsPipelineDesc meshPipelineDesc = GetDefaultGraphicsPipelineDesc();
			meshPipelineDesc.vertexShader = mMeshVertexShader.get();
			meshPipelineDesc.pixelShader = mMeshPixelShader.get();
			meshPipelineDesc.renderTargetDesc.numRenderTargets = 1;
			meshPipelineDesc.renderTargetDesc.renderTargetFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
			meshPipelineDesc.depthStencilDesc.DepthEnable = true;
			meshPipelineDesc.renderTargetDesc.depthStencilFormat = depthBufferDesc.resourceDesc.Format;
			meshPipelineDesc.depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

			mMeshPerObjectResourceSpace.SetCBV(mMeshConstantBuffers[0].get());
			mMeshPerObjectResourceSpace.Lock();

			mMeshPerPassResourceSpace.SetCBV(mMeshPassConstantBuffer.get());
			mMeshPerPassResourceSpace.Lock();

			PipelineResourceLayout meshResourceLayout;
			meshResourceLayout.spaces[PER_OBJECT_SPACE] = &mMeshPerObjectResourceSpace;
			meshResourceLayout.spaces[PER_PASS_SPACE] = &mMeshPerPassResourceSpace;

			mMeshPSO = CreateGraphicsPipeline(meshPipelineDesc, meshResourceLayout);
		}

		while (!engine.IsShouldClose())
		{
			engine.BeginFrame();

			TextureResource& backBuffer = gRHI.GetCurrentBackBuffer();

			graphicsContext->Reset();
			graphicsContext->AddBarrier(backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
			graphicsContext->AddBarrier(*mDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE);
			graphicsContext->FlushBarriers();

			graphicsContext->ClearRenderTarget(backBuffer, glm::vec4(0.3f, 0.3f, 0.8f, 1.0f));
			graphicsContext->ClearDepthStencilTarget(*mDepthBuffer, 1.0f, 0);

			static float rotation = 0.0f;
			rotation += 0.01f;

			if (mMeshVertexBuffer->isReady && mWoodTexture->isReady)
			{
				MeshConstants meshConstants;
				meshConstants.vertexBufferIndex = mMeshVertexBuffer->descriptorHeapIndex;
				meshConstants.textureIndex = mWoodTexture->descriptorHeapIndex;
				meshConstants.worldMatrix = glm::rotate(glm::mat4(1.0f), rotation, glm::vec3(0.0f, 1.0f, 0.0f));

				mMeshConstantBuffers[gRHI.GetFrameId()]->SetMappedData(&meshConstants, sizeof(MeshConstants));

				mMeshPerObjectResourceSpace.SetCBV(mMeshConstantBuffers[gRHI.GetFrameId()].get());

				PipelineInfo pipeline;
				pipeline.pipeline = mMeshPSO.get();
				pipeline.renderTargets.push_back(&backBuffer);
				pipeline.depthStencilTarget = mDepthBuffer.get();

				graphicsContext->SetPipeline(pipeline);
				graphicsContext->SetPipelineResources(PER_OBJECT_SPACE, mMeshPerObjectResourceSpace);
				graphicsContext->SetPipelineResources(PER_PASS_SPACE, mMeshPerPassResourceSpace);
				graphicsContext->SetDefaultViewPortAndScissor(rhi.GetFrameBufferSize());
				graphicsContext->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				graphicsContext->Draw(36);
			}

			graphicsContext->AddBarrier(backBuffer, D3D12_RESOURCE_STATE_PRESENT);
			graphicsContext->FlushBarriers();

			SubmitContextWork(*graphicsContext);

			engine.EndFrame();
		}

		DestroyPipelineStateObject(std::move(mMeshPSO));
		DestroyShader(std::move(mMeshPixelShader));
		DestroyShader(std::move(mMeshVertexShader));
		DestroyBuffer(std::move(mMeshVertexBuffer));
		DestroyBuffer(std::move(mMeshPassConstantBuffer));
		for (size_t i = 0; i < NUM_FRAMES_IN_FLIGHT; i++)
		{
			DestroyBuffer(std::move(mMeshConstantBuffers[i]));
		}
		DestroyTexture(std::move(mDepthBuffer));
		DestroyTexture(std::move(mWoodTexture));
	}
	engine.Destroy();
}