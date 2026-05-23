SYSROOT=/home/luckfox/Luckfox/luckfox-pico/sysdrv/source/buildroot/buildroot-2023.02.6/output/host/arm-buildroot-linux-uclibcgnueabihf/sysroot
TOOLCHAIN=/home/luckfox/Luckfox/luckfox-pico/tools/linux/toolchain/arm-rockchip830-linux-uclibcgnueabihf/bin

export PATH=$TOOLCHAIN:$PATH

# 清理旧残留，避免缓存干扰
rm -f moc_watchface.cpp moc_power.cpp watch_app

moc watchface.h -o moc_watchface.cpp
moc power.h -o moc_power.cpp

# 交叉编译
arm-rockchip830-linux-uclibcgnueabihf-g++ \
watch_main.cpp watchface.cpp power.cpp \
moc_watchface.cpp moc_power.cpp \
--sysroot="$SYSROOT" \
-I"$SYSROOT"/usr/include \
-I"$SYSROOT"/usr/include/qt5 \
-I"$SYSROOT"/usr/include/qt5/QtCore \
-I"$SYSROOT"/usr/include/qt5/QtGui \
-I"$SYSROOT"/usr/include/qt5/QtWidgets \
-L"$SYSROOT"/usr/lib \
-lQt5Core -lQt5Gui -lQt5Widgets \
-o watch_app
