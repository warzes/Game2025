#include "stdafx.h"

namespace e005
{
	static DirectX::XMFLOAT4X4 Identity4x4()
	{
		static DirectX::XMFLOAT4X4 I(
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f);

		return I;
	}

	struct ObjectConstants
	{
		DirectX::XMFLOAT4X4 WorldViewProj = Identity4x4();
	};
}

void ExampleRender005()
{
	EngineAppCreateInfo engineAppCreateInfo{};
	EngineApp engine;
	if (engine.Create(engineAppCreateInfo))
	{
		auto commandList = gRHI.GetCommandList();
		commandList->Reset(gRHI.GetCurrentCommandAllocator(), nullptr);

		struct Vertex
		{
			DirectX::XMFLOAT3 position;
			DirectX::XMFLOAT4 color;
		};

		// Create an root signature.
		ComPtr<ID3D12RootSignature> rootSignature;
		{
			// Shader programs typically require resources as input (constant buffers, textures, samplers).  The root signature defines the resources the shader programs expect.  If we think of the shader programs as a function, and the input resources as function parameters, then the root signature can be thought of as defining the function signature.
			// Root parameter can be a table, root descriptor or root constants.
			CD3DX12_ROOT_PARAMETER slotRootParameter[1];

			// Create a single descriptor table of CBVs.
			CD3DX12_DESCRIPTOR_RANGE cbvTable;
			cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
			slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable);

			// A root signature is an array of root parameters.
			CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc(1, slotRootParameter, 0, nullptr,
				D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

			ComPtr<ID3DBlob> signature;
			ComPtr<ID3DBlob> error;
			HRESULT result = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
			const char* err = "";
			if (!SUCCEEDED(result))
			{
				err = (char*)error->GetBufferPointer();
				Fatal("D3D12SerializeRootSignature failed: " + std::string(err));
			}
			gRHI.GetD3DDevice()->CreateRootSignature(
				0, 
				signature->GetBufferPointer(),
				signature->GetBufferSize(),
				IID_PPV_ARGS(&rootSignature));
		}

		// Create the pipeline state, which includes compiling and loading shaders.
		ComPtr<ID3D12PipelineState> pipelineState;
		{
			ComPtr<ID3DBlob> vertexShader = CompileShader(L"Data/Shaders/005_Render_Cube.hlsl", nullptr, "VSMain", "vs_5_0");
			ComPtr<ID3DBlob> pixelShader = CompileShader(L"Data/Shaders/005_Render_Cube.hlsl", nullptr, "PSMain", "ps_5_0");

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
			psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
			psoDesc.SampleMask = UINT_MAX;
			psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			psoDesc.NumRenderTargets = 1;
			psoDesc.RTVFormats[0] = gRHI.GetBackBufferFormat();
			psoDesc.SampleDesc.Count = 1;
			psoDesc.DSVFormat = gRHI.GetDepthStencilFormat();
			gRHI.GetD3DDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState));
		}
			
		// Create the constant buffer
		ConstantBufferD3D12 constantBuffer;
		DescriptorHandleD3D12 cbDescriptor = gRHI.descriptorHeapManager.CreateGPUHandle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 0, 1);
		{
			ConstantBufferCreateInfo cbci{};
			cbci.size = sizeof(e005::ObjectConstants);
			cbci.descriptor = cbDescriptor;
			constantBuffer.Create(cbci);
		}

		// Create geometry
		MeshGeometry boxGeo;
		{
			std::array<Vertex, 8> vertices =
			{
				Vertex({ DirectX::XMFLOAT3(-1.0f, -1.0f, -1.0f), DirectX::XMFLOAT4(DirectX::Colors::White) }),
				Vertex({ DirectX::XMFLOAT3(-1.0f, +1.0f, -1.0f), DirectX::XMFLOAT4(DirectX::Colors::Black) }),
				Vertex({ DirectX::XMFLOAT3(+1.0f, +1.0f, -1.0f), DirectX::XMFLOAT4(DirectX::Colors::Red) }),
				Vertex({ DirectX::XMFLOAT3(+1.0f, -1.0f, -1.0f), DirectX::XMFLOAT4(DirectX::Colors::Green) }),
				Vertex({ DirectX::XMFLOAT3(-1.0f, -1.0f, +1.0f), DirectX::XMFLOAT4(DirectX::Colors::Blue) }),
				Vertex({ DirectX::XMFLOAT3(-1.0f, +1.0f, +1.0f), DirectX::XMFLOAT4(DirectX::Colors::Yellow) }),
				Vertex({ DirectX::XMFLOAT3(+1.0f, +1.0f, +1.0f), DirectX::XMFLOAT4(DirectX::Colors::Cyan) }),
				Vertex({ DirectX::XMFLOAT3(+1.0f, -1.0f, +1.0f), DirectX::XMFLOAT4(DirectX::Colors::Magenta) })
			};

			std::array<std::uint16_t, 36> indices =
			{
				// front face
				0, 1, 2,
				0, 2, 3,

				// back face
				4, 6, 5,
				4, 7, 6,

				// left face
				4, 5, 1,
				4, 1, 0,

				// right face
				3, 2, 6,
				3, 6, 7,

				// top face
				1, 5, 6,
				1, 6, 2,

				// bottom face
				4, 0, 3,
				4, 3, 7
			};

			const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
			const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

			D3DCreateBlob(vbByteSize, &boxGeo.VertexBufferCPU);
			CopyMemory(boxGeo.VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

			D3DCreateBlob(ibByteSize, &boxGeo.IndexBufferCPU);
			CopyMemory(boxGeo.IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

			boxGeo.VertexBufferGPU = CreateDefaultBuffer(gRHI.GetD3DDevice(), commandList, vertices.data(), vbByteSize, boxGeo.VertexBufferUploader);

			boxGeo.IndexBufferGPU = CreateDefaultBuffer(gRHI.GetD3DDevice(), commandList, indices.data(), ibByteSize, boxGeo.IndexBufferUploader);

			boxGeo.VertexByteStride = sizeof(Vertex);
			boxGeo.VertexBufferByteSize = vbByteSize;
			boxGeo.IndexFormat = DXGI_FORMAT_R16_UINT;
			boxGeo.IndexBufferByteSize = ibByteSize;

			SubmeshGeometry submesh;
			submesh.IndexCount = (UINT)indices.size();
			submesh.StartIndexLocation = 0;
			submesh.BaseVertexLocation = 0;

			boxGeo.DrawArgs["box"] = submesh;
		}

		DirectX::XMFLOAT4X4 mWorld = e005::Identity4x4();
		DirectX::XMFLOAT4X4 mView = e005::Identity4x4();
		DirectX::XMFLOAT4X4 mProj = e005::Identity4x4();

		float mTheta = 1.5f * DirectX::XM_PI;
		float mPhi = DirectX::XM_PIDIV4;
		float mRadius = 5.0f;

		POINT mLastMousePos;

		const glm::vec4 clearColor = { 0.4f, 0.6f, 0.9f, 1.0f };

		// Close the command list and execute it to begin the initial GPU setup.
		commandList->Close();
		gRHI.graphicsQueue.ExecuteCommandList(commandList);
		gRHI.WaitForGpu();

		while (!engine.IsShouldClose())
		{
			engine.BeginFrame();

			// Update
			{
				PIXBeginEvent(PIX_COLOR_DEFAULT, L"Update");
				{
					// input
					{
						auto pos = engine.GetInputSystem().GetMousePosition();

						static bool mouseVisible = true;
	
						if (mouseVisible &&
							(engine.GetInputSystem().IsPress(MouseButton::Left) ||
							engine.GetInputSystem().IsPress(MouseButton::Right)))
						{
							mouseVisible = false;
							mLastMousePos.x = pos.x;
							mLastMousePos.y = pos.y;
							engine.GetInputSystem().SetMouseVisible(false);
						}
						else
						{
							mouseVisible = true;
							engine.GetInputSystem().SetMouseVisible(true);
						}

						if (engine.GetInputSystem().IsPress(MouseButton::Left))
						{
							// Make each pixel correspond to a quarter of a degree.
							float dx = DirectX::XMConvertToRadians(0.25f * static_cast<float>(pos.x - mLastMousePos.x));
							float dy = DirectX::XMConvertToRadians(0.25f * static_cast<float>(pos.y - mLastMousePos.y));

							// Update angles based on input to orbit camera around box.
							mTheta += dx;
							mPhi += dy;

							// Restrict the angle mPhi.
							mPhi = glm::clamp(mPhi, 0.1f, DirectX::XM_PI - 0.1f);
						}
						if (engine.GetInputSystem().IsPress(MouseButton::Right))
						{
							// Make each pixel correspond to 0.005 unit in the scene.
							float dx = 0.005f * static_cast<float>(pos.x - mLastMousePos.x);
							float dy = 0.005f * static_cast<float>(pos.y - mLastMousePos.y);

							// Update the camera radius based on input.
							mRadius += dx - dy;

							// Restrict the radius.
							mRadius = glm::clamp(mRadius, 3.0f, 15.0f);
						}
						mLastMousePos.x = pos.x;
						mLastMousePos.y = pos.y;
					}

					const float AspectRatio = (float)engine.GetWindowSystem().GetWidth() / (float)engine.GetWindowSystem().GetHeight();
					const float Pi = 3.1415926535f;
					DirectX::XMMATRIX P = DirectX::XMMatrixPerspectiveFovLH(0.25f * Pi, AspectRatio, 1.0f, 1000.0f);
					XMStoreFloat4x4(&mProj, P);

					// Convert Spherical to Cartesian coordinates.
					float x = mRadius * sinf(mPhi) * cosf(mTheta);
					float z = mRadius * sinf(mPhi) * sinf(mTheta);
					float y = mRadius * cosf(mPhi);

					// Build the view matrix.
					DirectX::XMVECTOR pos = DirectX::XMVectorSet(x, y, z, 1.0f);
					DirectX::XMVECTOR target = DirectX::XMVectorZero();
					DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

					DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(pos, target, up);
					DirectX::XMStoreFloat4x4(&mView, view);

					DirectX::XMMATRIX world = DirectX::XMLoadFloat4x4(&mWorld);
					DirectX::XMMATRIX proj = DirectX::XMLoadFloat4x4(&mProj);
					DirectX::XMMATRIX worldViewProj = world * view * proj;

					// Update the constant buffer with the latest worldViewProj matrix.
					e005::ObjectConstants objConstants;
					DirectX::XMStoreFloat4x4(&objConstants.WorldViewProj, DirectX::XMMatrixTranspose(worldViewProj));
					constantBuffer.CopyData(objConstants);
				}
				PIXEndEvent();
			}

			// Render
			{
				gRHI.Prepare();

				gRHI.ClearFrameBuffer(clearColor);

				PIXBeginEvent(commandList, PIX_COLOR_DEFAULT, L"Render");
				{
					commandList->SetPipelineState(pipelineState.Get());
					commandList->SetGraphicsRootSignature(rootSignature.Get());

					ID3D12DescriptorHeap* ppHeaps[] = { gRHI.descriptorHeapManager.GetGPUHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 0)->GetD3DHeap().Get()};
					commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

					commandList->SetGraphicsRootSignature(rootSignature.Get());

					auto vbv = boxGeo.VertexBufferView();
					commandList->IASetVertexBuffers(0, 1, &vbv);
					auto ibv = boxGeo.IndexBufferView();
					commandList->IASetIndexBuffer(&ibv);
					commandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

					commandList->SetGraphicsRootDescriptorTable(0, cbDescriptor.GPUHandle);

					commandList->DrawIndexedInstanced(boxGeo.DrawArgs["box"].IndexCount, 1, 0, 0, 0);
				}
				PIXEndEvent(commandList);

				gRHI.Present();
			}

			engine.EndFrame();
		}

		//vertexBuffer.Destroy();
		rootSignature.Reset();
		pipelineState.Reset();
	}
	engine.Destroy();
}