﻿-------------------------------------------------------------------------------
Списки команд
-------------------------------------------------------------------------------
Логика следующая:
- в D3D11 команды отправлялись в ID3D11DeviceContext и неявно выполнялись видеокартой как можно скорее.
- в D3D12 вместо этого класса появилось понятие списка команд.
- Команды пишутся в этот список. И затем помещаются в очередь команд. Именно очередь команд выполняет эти команды на GPU
- Для одного потока может хватить одного списка команд
- Очередь выполняет список команд по порядку. Но не гарантируется, что несколько очередей будут синхронизированы между собой. Если это нужно - нужно явно выполнять синхронизацию

ID3D12GraphicsCommandList - список команд.

ID3D12CommandAllocator - служит аллокатором памяти для записи команд в список команд. В отличие от списка команд, аллокатор не может повторно использоваться, пока все команды не выполнятся на GPU (выдаст ошибку COMMAND_ALLOCATOR_SYNC). Поэтому стоит создавать аллокатор на каждый фрейм в свапчейне

ID3D12CommandQueue - хранит очередь команд. Необходима для отправки списка команд на графический процессор.

-------------------------------------------------------------------------------
Дескрипторы
-------------------------------------------------------------------------------
ID3D12DescriptorHeap - массив дескрипторов (Descriptor) , где каждый дескриптор описывает ресурс в памяти GPU.
Размер дескриптора зависит от вендора. Нужно запрашивать его.
Дескрипторы немного похожи на resource views в D3D11.

-------------------------------------------------------------------------------
Root signature
-------------------------------------------------------------------------------
В D3D11 ресурсы привязывались к device context. В D3D12 ресурсы теперь привязываются к Root signature.
Root signature - это карта указывающая какие типы дескрипторов содержатся в каком местоположении
Если раньше нужно было усстанавливтаь все ресурсы по отдельности, то теперь достаточно биндить только root signature

-------------------------------------------------------------------------------
Root constants
-------------------------------------------------------------------------------
Более быстрый аналог отправки юниформ в шейдеры?




Текстуры свапчейна описываются render target view (RTV), который описывает расположение текстуры в GPU, ее размеры и формат. Хранится в ID3D12DescriptorHeap. Так как свапчейн хранит несколько текстур, то для каждой текстуры нужен свой дескриптор

