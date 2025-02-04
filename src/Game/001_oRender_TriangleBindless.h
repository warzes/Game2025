﻿#pragma once

void ExampleRender001()
{
	EngineAppCreateInfo engineAppCreateInfo{};
	EngineApp engine;
	if (engine.Create(engineAppCreateInfo))
	{
		auto& rhi = engine.GetRenderSystem();

		struct TriangleVertex
		{
			glm::vec2 position;
			glm::vec3 color; //You can/should pack a color in a uint32_t and unpack it in the shader, this is a Vector3 for simplicity
		};

		struct TriangleConstants
		{
			uint32_t vertexBufferIndex;
		};

		std::unique_ptr<BufferResource> mTriangleVertexBuffer;
		std::unique_ptr<BufferResource> mTriangleConstantBuffer;
		std::unique_ptr<Shader> mTriangleVertexShader;
		std::unique_ptr<Shader> mTrianglePixelShader;
		std::unique_ptr<PipelineStateObject> mTrianglePSO;
		PipelineResourceSpace mTrianglePerObjectSpace;

		// InitializeTriangleResources
		{
			std::array<TriangleVertex, 3> vertices;
			vertices[0].position = { -0.5f, -0.5f };
			vertices[0].color = { 1.0f, 0.0f, 0.0f };
			vertices[1].position = { 0.0f, 0.5f };
			vertices[1].color = { 0.0f, 1.0f, 0.0f };
			vertices[2].position = { 0.5f, -0.5f };
			vertices[2].color = { 0.0f, 0.0f, 1.0f };

			BufferCreationDesc triangleBufferDesc{};
			triangleBufferDesc.size = sizeof(vertices);
			triangleBufferDesc.accessFlags = BufferAccessFlags::hostWritable;
			triangleBufferDesc.viewFlags = BufferViewFlags::srv;
			triangleBufferDesc.stride = sizeof(TriangleVertex);
			triangleBufferDesc.isRawAccess = true;

			mTriangleVertexBuffer = CreateBuffer(triangleBufferDesc);
			mTriangleVertexBuffer->SetMappedData(&vertices, sizeof(vertices));

			BufferCreationDesc triangleConstantDesc{};
			triangleConstantDesc.size = sizeof(TriangleConstants);
			triangleConstantDesc.accessFlags = BufferAccessFlags::hostWritable;
			triangleConstantDesc.viewFlags = BufferViewFlags::cbv;

			TriangleConstants triangleConstants;
			triangleConstants.vertexBufferIndex = mTriangleVertexBuffer->descriptorHeapIndex;

			mTriangleConstantBuffer = CreateBuffer(triangleConstantDesc);
			mTriangleConstantBuffer->SetMappedData(&triangleConstants, sizeof(TriangleConstants));

			ShaderCreationDesc triangleShaderVSDesc;
			triangleShaderVSDesc.shaderName = L"Triangle.hlsl";
			triangleShaderVSDesc.entryPoint = L"VertexShader";
			triangleShaderVSDesc.type = ShaderType::vertex;

			ShaderCreationDesc triangleShaderPSDesc;
			triangleShaderPSDesc.shaderName = L"Triangle.hlsl";
			triangleShaderPSDesc.entryPoint = L"PixelShader";
			triangleShaderPSDesc.type = ShaderType::pixel;

			mTriangleVertexShader = CreateShader(triangleShaderVSDesc);
			mTrianglePixelShader = CreateShader(triangleShaderPSDesc);

			mTrianglePerObjectSpace.SetCBV(mTriangleConstantBuffer.get());
			mTrianglePerObjectSpace.Lock();

			PipelineResourceLayout resourceLayout;
			resourceLayout.spaces[PER_OBJECT_SPACE] = &mTrianglePerObjectSpace;

			GraphicsPipelineDesc trianglePipelineDesc = GetDefaultGraphicsPipelineDesc();
			trianglePipelineDesc.vertexShader = mTriangleVertexShader.get();
			trianglePipelineDesc.pixelShader = mTrianglePixelShader.get();
			trianglePipelineDesc.renderTargetDesc.numRenderTargets = 1;
			trianglePipelineDesc.renderTargetDesc.renderTargetFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

			mTrianglePSO = CreateGraphicsPipeline(trianglePipelineDesc, resourceLayout);
		}

		while (!engine.IsShouldClose())
		{
			engine.BeginFrame();

			TextureResource& backBuffer = gRHI.GetCurrentBackBuffer();

			gRHI.graphicsContext->Reset();
			gRHI.graphicsContext->AddBarrier(backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
			gRHI.graphicsContext->FlushBarriers(); // если нужно несколько transition ресурсов, то их нужно вызвать до Flush

			gRHI.graphicsContext->ClearRenderTarget(backBuffer, glm::vec4(0.3f, 0.3f, 0.8f, 1.0f));

			PipelineInfo pipeline;
			pipeline.pipeline = mTrianglePSO.get();
			pipeline.renderTargets.push_back(&backBuffer);

			gRHI.graphicsContext->SetPipeline(pipeline);
			gRHI.graphicsContext->SetPipelineResources(PER_OBJECT_SPACE, mTrianglePerObjectSpace);
			gRHI.graphicsContext->SetDefaultViewPortAndScissor(rhi.GetFrameBufferSize());
			gRHI.graphicsContext->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			gRHI.graphicsContext->Draw(3);

			gRHI.graphicsContext->AddBarrier(backBuffer, D3D12_RESOURCE_STATE_PRESENT);
			gRHI.graphicsContext->FlushBarriers();

			SubmitContextWork(*gRHI.graphicsContext);

			engine.EndFrame();
		}

		DestroyPipelineStateObject(std::move(mTrianglePSO));
		DestroyShader(std::move(mTriangleVertexShader));
		DestroyShader(std::move(mTrianglePixelShader));
		DestroyBuffer(std::move(mTriangleVertexBuffer));
		DestroyBuffer(std::move(mTriangleConstantBuffer));
	}
	engine.Destroy();
}