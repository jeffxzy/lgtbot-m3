EXTEND_OPTION("每回合最长时间x秒", 局时, (ArithChecker<uint32_t>(10, 3600, "局时（秒）")), 120)
EXTEND_OPTION("随机种子", 种子, (AnyArg("种子", "我是随便输入的一个字符串")), "")
EXTEND_OPTION("最大回合数", 回合数, (ArithChecker<uint32_t>(10, 100, "回合数")), 40)
EXTEND_OPTION("卡片颜色种类数量", 颜色, (ArithChecker<uint32_t>(1, 6, "数量")), 6)
EXTEND_OPTION("卡片点数种类数量", 点数, (ArithChecker<uint32_t>(1, 6, "数量")), 6)
EXTEND_OPTION("相同卡片的数量", 副数, (ArithChecker<uint32_t>(0, 10, "数量")), 0)
EXTEND_OPTION("游戏模式（具体含义参见游戏规则）", 模式, (BoolChecker("竞技", "经典")), false)
