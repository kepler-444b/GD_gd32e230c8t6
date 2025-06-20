### 					PCL 面板通信协议

#### 设置串码

参考[面板按键配置解析V3.0](https://docs.qq.com/doc/DVE1YdEhNWXlXa2lC?u=01de59d25c614f2886192fca680d7f21&nlc=1)

#### 通信协议

|  0xf1  | 按键功能 | 操作指令 | 分组  | 总关场景区域 | 校验码 | 按键权限 | 场景分组 |
| :----: | :------- | -------- | ----- | ------------ | ------ | -------- | -------- |
| 固定码 | 1字节    | 1字节    | 1字节 | 1字节        | 1字节  | 1字节    | 1字节    |

按下按键，会根据设置的串码组包后发送，组包方式可参考**强电485开关面板触发型通信协议**，通信帧在PLC总线上广播，若其他面板收到通信帧会根据其内容以及本身设置的串码来进行联动，以下解释面板在收到不同按键功能的通信帧后的处理流程

##### 总关（0x00）：

设备在收到“按键功能”为0x00后，需要匹配的信息有：

“总关区域”相同或“0xF”，“睡眠”被勾选

若满足以上条件，则执行以下动作

1. 联动双控：查找自身按键，若有“总关”且“双控分组”相同，则根据“操作指令”执行“总关”动作（白灯灯珠亮0.1s后熄灭，继电器动作）

2. 根据“操作指令”联动：查找自身的“夜灯” “勿扰模式”，根据“操作指令”执行动作（白色灯珠和继电器动作）

3. 直接关闭：查找自身的“场景模式”“灯控模式“总开关”“清理房间”，直接关闭（白色灯珠熄灭，继电器断开）

   

##### 总开关（0x01）

设备在收到“按键功能”为0x01后，需要匹配的信息有：

“总关区域”相同或“0xF”，“总开关”被勾选

若满足以上条件，则执行以下动作

1. 联动双控：查找自身按键，若有“总关”且“双控分组”相同，则根据“操作指令”执行“总开关”动作（白色灯珠和继电器动作）

2. 根据“操作指令”联动：查找自身的“夜灯”“勿扰模式”，根据“操作指令”反向执行动作（白色灯珠和继电器动作），查找自身的“场景模式”“灯控模式“清理房间”，根据“操作指令”执行动作（白色灯珠和继电器动作），查找自身的“总关”，根据“操作指令”执行动作（白色灯珠亮0.1s后熄灭，继电器动作）

   

##### 清理房间（0x02）

设备在收到“按键功能”为0x02后，需要匹配的信息有：

“按键功能”相同，“双控分组”相同或“0xF”

若满足以上条件，则执行以下动作

1. 关闭勿扰：若“操作指令”为0x01，则查找自身的“请勿打扰”，如果找到且与“清理房间”在同一分组，则关闭“请勿打扰”（白色灯珠熄灭，继电器断开）
2. 联动双控：根据“操作指令”执行动作（白色灯珠和继电器动作）

##### 勿扰模式（0x03）

设备在收到“按键功能”为0x03后，需要匹配的信息有：

“按键功能”相同，“双控分组”相同或“0xF”

若满足以上条件，则执行以下动作

1. 关闭清理：若“操作指令”为0x01，则查找自身的“清理房间”，如果找到且与“勿扰模式”在同一分组，则关闭“清理房间”（白色灯珠熄灭，继电器断开）
2. 联动双控：根据“操作指令”执行动作（白色灯珠和继电器动作）

##### 请稍后（0x04）

设备在收到“按键功能”为0x04后，需要匹配的信息有：

“按键功能”相同，“双控分组”相同或“0xF“

若满足以上条件，则执行以下动作

1. 联动双控：根据“操作指令”执行动作（白色灯珠点亮，继电器导通， 30s后白色灯珠熄灭，继电器断开）

##### 退房（0x05）

设备在收到“按键功能”为0x05后，需要匹配的信息有：

“按键功能”相同，“双控分组”相同或“0xF”

若满足以上条件，则执行以下动作

1. 联动双控：根据“操作指令”执行动作（白色灯珠和继电器动作）

##### 紧急呼叫（0x06）

设备在收到“按键功能”为0x06后，需要匹配的信息有：

“按键功能”相同，“双控分组”相同或“0xF”

若满足以上条件，则执行以下动作

1. 联动双控：根据“操作指令”执行动作（白色灯珠和继电器动作）

##### 请求服务（0x07）

设备在收到“按键功能”为0x07后，需要匹配的信息有：

“按键功能”相同，“双控分组”相同或“0xF”

若满足以上条件，则执行以下操作

1. 联动双控：根据“操作指令”执行动作（白色灯珠和继电器动作）

##### 窗帘开（0x10）

设备在收到“按键功能”为0x10后，需要匹配的信息有：

“按键功能”相同，“双控分组”相同或“0xF”

若满足以上条件，则执行以下操作

1. 窗帘关：查找自身的“窗帘关”，如果找到且与“窗帘开”在同一分组，则关闭“窗帘关”的继电器
2. 联动双控：执行动作（白色灯珠亮0.1s，继电器导通30s）

##### 窗帘关（0x11）

设备在收到“按键功能”为0x11后，需要匹配的信息有：

“按键功能”相同，“双控分组”相同或“0xF”

若满足以上条件，则执行以下操作

1. 窗帘开：查找自身的“窗帘开”，如果找到且与“窗帘关”在同一分组，则关闭“窗帘开”的继电器
2. 联动双控：执行动作（白色灯珠亮0.1s，继电器导通30s）

##### 场景模式（0x0D）

设备在收到“按键功能”为0x0D后，需要匹配的信息有：

“场景分组”不为0x00、，“场景区域”相同或“0xF”

若满足以上条件，则执行以下操作

1. 关闭场景分组：关闭该场景区域的所有场景分组（白色灯珠熄灭，继电器断开）
2. 匹配场景分组：任意一位同为1,说明勾选了该场景分组,执行动作（白色灯珠点亮，继电器导通）

##### 灯控模式（0x0E）

设备在收到“按键功能”为0x0E后，需要匹配的信息有：

“按键功能”相同，“双控分组”相同或“0xF”

若满足以上条件，则执行以下操作

1. 联动双控：根据“操作指令”执行动作（白色灯珠和继电器动作）

##### 夜灯（0x0F）

设备在收到“按键功能”为0x0F后，需要匹配的信息有：

“按键功能”相同，“双控分组”相同或“0x0F”

若满足以上条件，则执行以下操作

1. 联动双控：根据“操作指令”执行动作（白色灯珠和继电器动作）

##### 窗帘停（0x62）

设备在收到“按键功能”为0x62后，需要匹配的信息有：

“按键功能”为“窗帘开”或“窗帘关”，“双控分组”相同或“0xF”，继电器正在导通中

若满足以上条件，则执行以下操作

1. 白的灯珠点亮0.1s，关闭继电器
