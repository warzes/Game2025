﻿#include "stdafx.h"

void ExampleRender001()
{
	EngineAppCreateInfo engineAppCreateInfo{};
	EngineApp engine;
	if (engine.Create(engineAppCreateInfo))
	{
		struct Vertex
		{
			DirectX::XMFLOAT3 position;
			DirectX::XMFLOAT4 color;
		};
		float aspectRatio = 800.0f / 600.0f;
		// Define the geometry for a triangle.
		Vertex triangleVertices[] =
		{
			{ {  0.0f,   0.25f * aspectRatio, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
			{ {  0.25f, -0.25f * aspectRatio, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
			{ { -0.25f, -0.25f * aspectRatio, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
		};

		const UINT vertexBufferSize = sizeof(triangleVertices);

		// Create an empty root signature.
		ComPtr<ID3D12RootSignature> rootSignature;
		{
			D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
			rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

			ComPtr<ID3DBlob> signature;
			ComPtr<ID3DBlob> error;
			HRESULT result = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
			const char* err = "";
			if (!SUCCEEDED(result))
			{
				err = (char*)error->GetBufferPointer();
				Fatal("D3D12SerializeRootSignature failed: " + std::string(err));
			}
			gRHI.GetD3DDevice()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
		}

		// Create the pipeline state, which includes compiling and loading shaders.
		ComPtr<ID3D12PipelineState> pipelineState;
		{
			ComPtr<ID3DBlob> vertexShader;
			ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
			// Enable better shader debugging with the graphics debugging tools.
			UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
			UINT compileFlags = 0;
#endif

			D3DCompileFromFile(L"Data/Shaders/001_Render_Triangle.hlsl", nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr);
			D3DCompileFromFile(L"Data/Shaders/001_Render_Triangle.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr);

			// Define the vertex input layout.
			D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
			};

			// Describe and create the graphics pipeline state object (PSO).
			D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
			psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
			psoDesc.pRootSignature = rootSignature.Get();
			psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
			psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
			psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			psoDesc.DepthStencilState.DepthEnable = FALSE;
			psoDesc.DepthStencilState.StencilEnable = FALSE;
			psoDesc.SampleMask = UINT_MAX;
			psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			psoDesc.NumRenderTargets = 1;
			psoDesc.RTVFormats[0] = gRHI.GetBackBufferFormat();
			psoDesc.SampleDesc.Count = 1;
			gRHI.GetD3DDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState));
		}

		// Create the vertex buffer.
		VertexBufferD3D12 vertexBuffer;
		{
			VertexBufferCreateInfo vbci{};
			vbci.vertexSize = sizeof(Vertex);
			vbci.vertexCount = 3;
			vertexBuffer.Create(vbci);
			UINT8* pVertexDataBegin = vertexBuffer.Map();
			memcpy(pVertexDataBegin, triangleVertices, sizeof(triangleVertices));
			vertexBuffer.Unmap();
		}
		const glm::vec4 clearColor = { 0.4f, 0.6f, 0.9f, 1.0f };

		auto commandList = gRHI.GetCommandList();

		while (!engine.IsShouldClose())
		{
			engine.BeginFrame();

			// Update
			{
				PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");
				{
					// TODO: Add your logic here.
				}
				PIXEndEvent();
			}

			// Render
			{
				gRHI.Prepare();

				gRHI.ClearFrameBuffer(clearColor);

				PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render");
				{
					commandList->SetGraphicsRootSignature(rootSignature.Get());
					commandList->SetPipelineState(pipelineState.Get());
					commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
					commandList->IASetVertexBuffers(0, 1, &vertexBuffer.View());
					commandList->DrawInstanced(3, 1, 0, 0);
				}
				PIXEndEvent(commandList);

				gRHI.Present();
			}

			engine.EndFrame();
		}

		vertexBuffer.Destroy();
		rootSignature.Reset();
		pipelineState.Reset();
	}
	engine.Destroy();
}