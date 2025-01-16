#include "stdafx.h"

using namespace DirectX;

void ExampleRender005()
{
	EngineAppCreateInfo engineAppCreateInfo{};
	EngineApp engine;
	if (engine.Create(engineAppCreateInfo))
	{
		struct Vertex
		{
			DirectX::XMFLOAT3 position;
			DirectX::XMFLOAT2 texCoord;
		};
		float aspectRatio = 800.0f / 600.0f;
		// Define the geometry for a triangle.
		Vertex triangleVertices[] =
		{
			{ { 0.0f, 0.25f * aspectRatio, 0.0f }, { 0.5f, 0.0f } },
			{ { 0.25f, -0.25f * aspectRatio, 0.0f }, { 1.0f, 1.0f } },
			{ { -0.25f, -0.25f * aspectRatio, 0.0f }, { 0.0f, 1.0f } }
		};

		const UINT vertexBufferSize = sizeof(triangleVertices);

		ComPtr<ID3D12DescriptorHeap> mainDescriptorHeap[MAX_BACK_BUFFER_COUNT];
		for (int i = 0; i < MAX_BACK_BUFFER_COUNT; ++i)
		{
			D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
			heapDesc.NumDescriptors = 2;
			heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			gRHI.GetD3DDevice()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mainDescriptorHeap[i]));
		}

		// create root signature
		ComPtr<ID3D12RootSignature> rootSignature;
		{
			D3D12_DESCRIPTOR_RANGE cbDescriptorRange;
			cbDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
			cbDescriptorRange.NumDescriptors = 1;
			cbDescriptorRange.BaseShaderRegister = 0;
			cbDescriptorRange.RegisterSpace = 0;
			cbDescriptorRange.OffsetInDescriptorsFromTableStart = 0;

			D3D12_DESCRIPTOR_RANGE textureDescRange;
			textureDescRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			textureDescRange.NumDescriptors = 1;
			textureDescRange.BaseShaderRegister = 0;
			textureDescRange.RegisterSpace = 0;
			textureDescRange.OffsetInDescriptorsFromTableStart = 1;

			D3D12_ROOT_PARAMETER  rootParameters[3];

			rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			rootParameters[0].DescriptorTable = { 1, &cbDescriptorRange };
			rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

			rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
			rootParameters[1].Descriptor = { 1, 0 };
			rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

			rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			rootParameters[2].DescriptorTable = { 1, &textureDescRange };
			rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

			// create a static sampler
			D3D12_STATIC_SAMPLER_DESC sampler = {};
			sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
			sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			sampler.MipLODBias = 0;
			sampler.MaxAnisotropy = 0;
			sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
			sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
			sampler.MinLOD = 0.0f;
			sampler.MaxLOD = D3D12_FLOAT32_MAX;
			sampler.ShaderRegister = 0;
			sampler.RegisterSpace = 0;
			sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

			D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
			rootSignatureDesc.NumParameters = _countof(rootParameters);
			rootSignatureDesc.pParameters = rootParameters;
			rootSignatureDesc.NumStaticSamplers = 1;
			rootSignatureDesc.pStaticSamplers = &sampler;
			rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

			ComPtr<ID3DBlob> signatureBlob;
			ComPtr<ID3DBlob> error;
			HRESULT result = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &error);
			const char* err = "";
			if (!SUCCEEDED(result))
			{
				err = (char*)error->GetBufferPointer();
				Fatal("D3D12SerializeRootSignature failed: " + std::string(err));
			}
			gRHI.GetD3DDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
		}

		// # CONSTANT BUFFER
		D3D12MA::Allocation* constantBufferUploadAllocation[MAX_BACK_BUFFER_COUNT];
		ComPtr<ID3D12Resource> constantBufferUploadHeap[MAX_BACK_BUFFER_COUNT];
		void* constantBufferAddress[MAX_BACK_BUFFER_COUNT];
		for (int i = 0; i < MAX_BACK_BUFFER_COUNT; ++i)
		{
			D3D12MA::ALLOCATION_DESC constantBufferUploadAllocDesc = {};
			constantBufferUploadAllocDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
			D3D12_RESOURCE_DESC constantBufferResourceDesc = {};
			constantBufferResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			constantBufferResourceDesc.Alignment = 0;
			constantBufferResourceDesc.Width = 1024 * 64;
			constantBufferResourceDesc.Height = 1;
			constantBufferResourceDesc.DepthOrArraySize = 1;
			constantBufferResourceDesc.MipLevels = 1;
			constantBufferResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
			constantBufferResourceDesc.SampleDesc.Count = 1;
			constantBufferResourceDesc.SampleDesc.Quality = 0;
			constantBufferResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			constantBufferResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
			gRHI.GetD3DAllocator()->CreateResource(
				&constantBufferUploadAllocDesc,
				&constantBufferResourceDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				&constantBufferUploadAllocation[i],
				IID_PPV_ARGS(&constantBufferUploadHeap[i]));
			constantBufferUploadHeap[i]->SetName(L"Constant Buffer Upload Resource Heap");
			constantBufferUploadAllocation[i]->SetName(L"Constant Buffer Upload Resource Heap");

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
			cbvDesc.BufferLocation = constantBufferUploadHeap[i]->GetGPUVirtualAddress();
			cbvDesc.SizeInBytes = AlignUp<UINT>(sizeof(ConstantBuffer0_PS), 256);
			gRHI.GetD3DDevice()->CreateConstantBufferView(&cbvDesc, mainDescriptorHeap[i]->GetCPUDescriptorHandleForHeapStart());

			constantBufferUploadHeap[i]->Map(0, &EMPTY_RANGE, &constantBufferAddress[i]);
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
		ComPtr<ID3D12Resource> vertexBuffer;
		{
			// Note: using upload heaps to transfer static data like vert buffers is not recommended. Every time the GPU needs it, the upload heap will be marshalled over. Please read up on Default Heap usage. An upload heap is used here for code simplicity and because there are very few verts to actually transfer.
			const auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			const auto desc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
			gRHI.GetD3DDevice()->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vertexBuffer));

			// Copy the triangle data to the vertex buffer.
			UINT8* pVertexDataBegin;
			CD3DX12_RANGE readRange(0, 0); // We do not intend to read from this resource on the CPU.
			vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin));
			memcpy(pVertexDataBegin, triangleVertices, sizeof(triangleVertices));
			vertexBuffer->Unmap(0, nullptr);
		}

		// Create the vertex buffer.
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
		// Initialize the vertex buffer view.
		vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
		vertexBufferView.StrideInBytes = sizeof(Vertex);
		vertexBufferView.SizeInBytes = vertexBufferSize;

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
					commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
					commandList->DrawInstanced(3, 1, 0, 0);
				}
				PIXEndEvent(commandList);

				gRHI.Present();
			}

			engine.EndFrame();
		}

		rootSignature.Reset();
		pipelineState.Reset();
		vertexBuffer.Reset();
	}
	engine.Destroy();
}