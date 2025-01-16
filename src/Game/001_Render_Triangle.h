#include "stdafx.h"

using namespace DirectX;

void ExampleRender001()
{
	EngineAppCreateInfo engineAppCreateInfo{};
	EngineApp engine;
	if (engine.Create(engineAppCreateInfo))
	{
		const FLOAT clearColor[] = { 0.4f, 0.6f, 0.9f, 1.0f };

		struct Vertex
		{
			XMFLOAT3 position;
			XMFLOAT4 color;
		};

		ComPtr<ID3D12Resource>      vertexBuffer;
		D3D12_VERTEX_BUFFER_VIEW    vertexBufferView;

		// Create an empty root signature.
		ComPtr<ID3D12RootSignature> rootSignature;
		{
			CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
			rootSignatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

			ComPtr<ID3DBlob> signature;
			ComPtr<ID3DBlob> error;
			D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
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
		{
			float aspectRatio = 800.0f / 600.0f;
			// Define the geometry for a triangle.
			Vertex triangleVertices[] =
			{
				{ {  0.0f,   0.25f * aspectRatio, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
				{ {  0.25f, -0.25f * aspectRatio, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
				{ { -0.25f, -0.25f * aspectRatio, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
			};

			const UINT vertexBufferSize = sizeof(triangleVertices);

			// Note: using upload heaps to transfer static data like vert buffers is not 
			// recommended. Every time the GPU needs it, the upload heap will be marshalled 
			// over. Please read up on Default Heap usage. An upload heap is used here for 
			// code simplicity and because there are very few verts to actually transfer.
			const auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			const auto desc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
			gRHI.GetD3DDevice()->CreateCommittedResource(
				&heapProp,
				D3D12_HEAP_FLAG_NONE,
				&desc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&vertexBuffer));

			// Copy the triangle data to the vertex buffer.
			UINT8* pVertexDataBegin;
			CD3DX12_RANGE readRange(0, 0); // We do not intend to read from this resource on the CPU.
			vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin));
			memcpy(pVertexDataBegin, triangleVertices, sizeof(triangleVertices));
			vertexBuffer->Unmap(0, nullptr);

			// Initialize the vertex buffer view.
			vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
			vertexBufferView.StrideInBytes = sizeof(Vertex);
			vertexBufferView.SizeInBytes = vertexBufferSize;
		}

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

				// Clear the render target.
				PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Clear");
				{
					const auto rtvDescriptor = gRHI.GetRenderTargetView();
					const auto dsvDescriptor = gRHI.GetDepthStencilView();
					const auto viewport = gRHI.GetScreenViewport();
					const auto scissorRect = gRHI.GetScissorRect();

					commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, &dsvDescriptor);
					commandList->ClearRenderTargetView(rtvDescriptor, clearColor, 0, nullptr);
					commandList->ClearDepthStencilView(dsvDescriptor, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
					commandList->RSSetViewports(1, &viewport);
					commandList->RSSetScissorRects(1, &scissorRect);
				}
				PIXEndEvent(commandList);

				PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render");
				{
					commandList->SetGraphicsRootSignature(rootSignature.Get());
					commandList->SetPipelineState(pipelineState.Get());
					commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
					commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
					commandList->DrawInstanced(3, 1, 0, 0);
				}
				PIXEndEvent(commandList);

				PIXBeginEvent(PIX_COLOR_DEFAULT, L"Present");
				gRHI.Present();
				PIXEndEvent();
			}

			engine.EndFrame();
		}

		rootSignature.Reset();
		pipelineState.Reset();
		vertexBuffer.Reset();
	}
	engine.Destroy();
}