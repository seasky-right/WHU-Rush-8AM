项目名称：WHU-Rush-8am (武大早八冲锋号) - 校园导航模拟器

1. 项目概述 (Overview)

本项目是一个基于 C++ 和 Qt 的校园导航与通勤模拟系统。旨在帮助武汉大学的学生（特别是面临早八跨校区通勤的学生）规划从宿舍到教学楼的最优路线。
系统不仅计算最短距离，更核心的是**“迟到预警”与“生活化决策”**，通过对比通勤时间与上课时间，结合天气、交通工具和途经点（如食堂），智能推荐最优路线。

2. 核心用户画像 (User Persona)

用户： 住在信息学部，却要去文理学部教五上早八的大一新生。

痛点： 怕迟到，不知道那条路最近，纠结是否要等校车，想顺路买早餐但怕来不及。

需求： 输入当前状态，系统直观给出“红绿灯”决策反馈和可视化的路径演示。

3. 功能需求 (Functional Requirements)

3.1 地图可视化与交互 (Map Visualization & Interaction)

底图策略 (Map Source):

素材: 采用高清静态截图（源自高德/百度自定义地图，去除干扰 POI，保留道路与建筑）。

比例尺 (Scale): 严格锁定 1 px = 0.91 米 (基于基准测试：图上 110px = 现实 100m)。

交互: 支持鼠标滚轮缩放、左键拖拽平移。

起终点输入模式 (Dual-Input System):

鼠标模式 (Click-to-Set):

左键点击地图 -> 设置“起点”，输入框回填，显示起点图标。

右键点击地图 -> 设置“终点”，输入框回填，显示终点图标。

键盘模式 (Type-to-Search):

智能联想 (Completer): 输入“教”，自动下拉“教一”、“教三”、“教五”。

严格验证: 输入未定义的地点时，输入框变红，无法开始。

途经点管理 (Waypoints System):

添加: 点击“+”按钮或地图右键菜单。

限制: 至多 3 个途经点，支持拖拽排序。

POI 与路径气泡交互 (Hover Interaction):

节点 (Node): 悬停放大 (1.2x) 并高亮，显示 [名称] + [描述]。

路径 (Edge): 悬停在特殊路段（如“樱花大道”、“绝望坡”）时，线条加粗变色，显示路名及坡度。

路径绘制动画 (Route Animation):

规划成功后，路线产生**“生长动画”**，线条从起点沿着路径平滑流向终点（时长 0.5s ~ 1s）。

3.2 交通工具与出行模式 (Transport Modes & Logic)

系统支持 5 种交通工具，严格遵循现实物理限制：

步行 (Walking):

速度: 1.2 m/s (4.3 km/h)。

路权: 全地形（可走楼梯、坡道、普通道路）。

共享电瓶车 (Shared E-bike) - 

$$混合模式$$

:

速度: 3.89 m/s (14.0 km/h) (受新国标电子限速)。

逻辑: 起点 -> (步行) -> 取车点 -> (骑行) -> 还车点 -> (步行) -> 终点。

自行车 (Bicycle):

速度: 4.2 m/s (15.1 km/h) (稍快于限速电瓶车)。

路权: 仅限道路，严禁走楼梯。

自用电瓶车 (Personal E-bike):

速度: 5.56 m/s (20.0 km/h) (最快个人交通工具)。

路权: 仅限道路。

校巴 (Campus Bus) - 

$$混合模式/时刻表$$

:

速度: 10.0 m/s (36 km/h)。

逻辑:

计算 Arrival_Station_Time = Current_Time + Walk_Duration(Start->Station).

查表算法: 读取 bus_schedule.csv，找到该车站 大于且最接近 Arrival_Station_Time 的班次 Bus_Depart_Time。

计算 Wait_Time = Bus_Depart_Time - Arrival_Station_Time.

若当天已无班次，则该模式不可用。

等车时间: 动态计算 (非固定)。

3.3 多策略推荐算法 (Multi-Strategy Recommendation)

基于交通工具、天气、途经点及海拔数据，系统每次计算都需尝试生成以下 3 条差异化路线：

分段计算核心: 若有途经点，公式为 Route = Path(Start→W1) + Path(W1→W2) + Path(W2→End)。

极限冲刺 (The Fastest) - 

$$物理最优$$

:

核心: “只要够快，再累也走”。

权重: Weight = Duration。

物理计算 (Directional Slope):

下坡 (Downhill): 车辆速度 x1.5 (冲坡加速)。

上坡 (Uphill, Slope > 5%): 自行车/电瓶车速度 x0.3 (推车/动力不足)，步行速度 x0.8。

结果: 即使需要爬绝望坡，只要总耗时最短，依然推荐。

懒人养生 (The Easiest) - 

$$心理最优$$

:

核心: “宁愿绕路，绝不爬坡”。

权重: Weight = Cost (心理代价)。

惩罚机制:

上坡: 权重 x20 (极度排斥)。

楼梯: 权重 x10。

结果: 自动绕开高阻力区域，生成平坦路线。

经济适用 (The Shortest) - 

$$距离最优$$

:

核心: “只看距离，不看路况”。

权重: Weight = Distance (米)。

限制: 强制锁定为 步行 模式。

结果: 往往是穿小路、走阶梯的直线路径。

3.4 出行模拟动画 (Travel Simulation)

Q版小人: 根据模式切换图标 (走路/骑车/校车)。

沿线移动: 严格吸附路径。

演示倍速 (Sim Rate): 60.0x (现实 1分钟 = 模拟 1秒)。

计算公式: Pixel_Step = (Real_Speed / 0.91) / 30_FPS * 60_Rate。

自动导览: 经过景点 (<20px) 自动触发气泡。

3.5 迟到监测模块 (Late Detection System)

输入: Current_Time, Class_Time, Weather.

计算: ETA = Current_Time + Travel_Duration + Dynamic_Wait_Time (基于时刻表) + Waypoint_Stay_Time (默认 5min/个) + Weather_Delay.

反馈 (Feedback Logic):

单卡片监测 (Card Level):

安全 (Safe): 若该路线 ETA <= Class_Time，卡片保持正常样式，显示绿色的“准点”标记。

迟到 (Late): 若该路线 ETA > Class_Time，该卡片背景直接标红，并显示“迟到 +xx 分钟”。

全局警告 (Global Alert):

条件: 若 所有 3 条 推荐路线均判定为“迟到”。

行为: 屏幕中央弹出红色模态警告框 (Modal Alert)。

3.6 天气影响模块 (Weather Impact System)

用户选择天气，影响全局算法权重：

晴天 (Sunny): 标准速度，时刻表准点。

雨天 (Rainy):

步行速度 -20%。

骑行模式增加 Safety_Penalty (权重 x1.5)。

校车延误: 所有时刻表班次推迟 5 min 发车 (Simulated Delay)。

大雪 (Snowy):

步行速度 -40%。

禁用所有自行车/电瓶车模式 (Weight = Infinity)。

楼梯权重 x100 (禁止通行)。

校车延误: 所有时刻表班次推迟 15 min 发车。

3.7 智能生活推荐 (Smart Lifestyle Recommendation)

逻辑: 若 (Class_Time - ETA) > 50 min (30min 余量 + 20min 吃饭) 且未经过食堂。

行为: 检索周边 200m (约 220px) 内 category="Canteen" 的节点。

交互: 弹出提示卡片，确认后自动添加食堂为途经点。

4. 技术架构 (Technical Architecture)

4.1 架构模式：MVVM (Model-View-ViewModel)

View: UI 渲染 (Qt Widgets + Graphics View)。

ViewModel: 状态管理、命令响应、数据转换。

Model: 纯 C++ 逻辑 (Dijkstra, 物理计算)。

4.2 核心类设计 (Class Design)

Layer 1: Model

Node: {id, name, x, y, z, type, description, category}

z (海拔): 文理学部正门 30m，樱顶 80m。

Edge: {u, v, weight, type, is_slope, name, description}

BusSchedule 

$$New Class$$

:

loadSchedule(csvFile)

getNextBusTime(stationId, currentTime)

GraphModel:

loadData(): 解析 txt/csv。

getEdgeCost(edge, mode, weather): 核心物理引擎，实现上述坡度/天气逻辑。

Layer 2: ViewModel

MainViewModel: 属性绑定 (isLate, weather), 命令接口 (addWaypoint).

Layer 3: View

MainWindow: 侧边栏布局, QCompleter.

MapWidget:

NodeItem: 悬停放大。

EdgeItem: 重写 shape() 扩大点击范围。

Animation: QVariantAnimation 控制生长/移动。

4.3 数据存储 (Data Persistence)

nodes.txt:

Format: id, name, x, y, z, type, description, category

Example: 101, 梅园食堂, 400, 300, 35.5, 0, 好吃的热干面, Canteen

edges.txt:

Format: u, v, distance, type, is_slope, name, description

Example: 10, 11, 200, 0, true, 绝望坡, 坡度很大谨防溜车

注意: distance 应为 Pixel_Distance * 0.91。

bus_schedule.csv 

$$New$$

:

Format: station_id, departure_time_1, departure_time_2, ...

Example: 201, 07:00, 07:15, 07:30, 07:45, 08:00

逻辑: 每一行代表一个车站的全天发车时间表。

5. 界面 Vibe (UI Design)

整体风格: 现代、扁平、干净。主色调为武大樱花粉 (#F8C3CD) 与科技灰 (#2C3E50)。

左侧控制栏: 天气下拉框 -> 路径输入(起/终/途) -> 时间设置 -> 监测按钮。

底部信息栏: 三张方案卡片。

反馈机制 (Feedback):

部分迟到: 迟到的卡片背景变红。

全部迟到: 屏幕中央弹出红色警告。

生活推荐: 智能推荐温馨提示。

6. 核心数值常量表 (Core Constants)

| 参数项 | 数值 | 说明 |
| MAP_SCALE | 0.91 | 米/像素 |
| SIMULATION_RATE | 60.0 | 演示倍速 |
| EATING_TIME | 1200 | 秒 (20分钟) |
| SLOPE_THRESHOLD | 0.05 | 坡度阈值 (5%) |
| SPEED_WALK | 1.2 | m/s |
| SPEED_BIKE | 4.2 | m/s |
| SPEED_EBIKE_SHARED | 3.89 | m/s |
| SPEED_EBIKE_OWN | 5.56 | m/s |5