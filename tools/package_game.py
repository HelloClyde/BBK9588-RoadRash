"""Package the unified BDA with the needed 3DO assets and one music track."""

from __future__ import annotations

import hashlib
import argparse
from pathlib import Path
import tarfile
import zipfile


ROOT = Path(__file__).resolve().parent.parent
BDA = ROOT / "RoadRash.bda"
PACK = ROOT / "Rash.pak"
SOURCE = ROOT / "local-data" / "3do-eu-extracted" / "Rash"
OUTPUT = ROOT / "RoadRash-9588-no-video.zip"
TEMP = ROOT / "RoadRash-9588-no-video.zip.tmp"
COMPACT_OUTPUT = ROOT / "RoadRash-9588-no-video.tar.xz"
COMPACT_TEMP = ROOT / "RoadRash-9588-no-video.tar.xz.tmp"
EXPECTED_RESOURCE_COUNT = 32
EXPECTED_RESOURCE_BYTES = 59_621_760
STREAM_FILES = {
    "Streams/bgaudio/RashIF.RSRC",
    "Streams/bgaudio/SG.RustyCage_sw22.stream",
}

INSTALL = """暴力摩托 9588 无视频整包

RoadRash.bda 是统一游戏程序，请按 9588 的常规 BDA 安装方式安装。
请先在电脑上解压 ZIP 或 tar.xz，不要把压缩包直接复制到设备。
将本压缩包中的单个 Rash.pak 放到 9588 的
B:\\应用\\数据\\游戏\\Rash\\Rash.pak。若没有 B: 盘，可改放到
A:\\应用\\数据\\游戏\\Rash\\Rash.pak。只需复制这一份资源文件，
无需把原版资源解压成几十个小文件。游戏优先读取 B: 的资源包，
没有时读取 A:；旧版散装资源仍可作为后备。

资源包含五条赛道所需文件和 Soundgarden《Rusty Cage》一首音乐，不含其他音乐及视频流。
原版资源来自用户提供的 3DO 镜像；本机主机测试与 BDA 包校验通过。
真机现已可进入赛道。本版将原版仪表适配至 320×240 底边，虚拟油门、甩鞭与踢腿按钮均为半透明；主角转向按原图比例缩放并补入中间帧，重撞后显示翻车、倒地、跑回摩托车并上车。效果仍需真机复测。
对手与行人使用同一透视基准并限制近处过度放大；碰撞时机按主角在画面中的位置校准。主角摔倒期间对手、交通和动态对象继续运动，真机观感仍待复测。
顺时针横握时，中文主菜单按实体左/右键选择“关卡选择”“设置”或“关于”，确认键进入。设置页用确认键切换音乐开关，关闭音乐仍保留音效；选择会保存在游戏存档中。返回键回到主菜单。关于页列有原版信息、移植作者 HelloClyde、赞助“唔识游水的鱼??”及步步高电子词典游戏群（830340878）。
选关页按实体左/右键换赛道，下/上键换摩托车。比赛中按实体下/上键向画面左/右转向；点击 GAS 切换油门开关，点击 HIT 甩鞭，点击 KICK 踢腿；也可按住画面下方向的实体右键再点 HIT 踢腿。确认键刹车；停止触屏片刻后按住返回键暂停。
工作电脑上的 G: 是 9288，不应作为 9588 的目标盘。

真机诊断日志写入选定资源目录的 RRDEBUG.LOG；若创建失败则尝试 A:\\RRDEBUG.LOG。
地图花屏或死机后，请重新连接设备读取日志；再次启动游戏会追加新的运行段。
若没有声音，请查看最近一次运行段的 SFX_DONE、MUSIC_DONE、AUDIO_OPEN、
AUDIO_WRITE 或 AUDIO_WAIT/AUDIO_WRITE_FAIL 记录。
比赛中的逐键、触屏和逐帧详细日志默认关闭，以免同步写盘打断音乐。
如需排查输入或渲染问题，请在实际读取的 Rash 目录中创建空文件 RRTRACE.ON，
重启游戏后复现并读取 RRDEBUG.LOG；排查结束后删除 RRTRACE.ON。
"""


def source_files() -> list[Path]:
    if not SOURCE.is_dir():
        raise SystemExit("Missing extracted Rash resource tree")
    files = []
    for path in SOURCE.rglob("*"):
        if not path.is_file():
            continue
        parts = path.relative_to(SOURCE).parts
        if "Streams" in parts and path.relative_to(SOURCE).as_posix() not in STREAM_FILES:
            continue
        files.append(path)
    files.sort(key=lambda path: path.relative_to(SOURCE).as_posix())
    count = len(files)
    size = sum(path.stat().st_size for path in files)
    if count != EXPECTED_RESOURCE_COUNT or size != EXPECTED_RESOURCE_BYTES:
        raise SystemExit(
            f"Unexpected no-video resource set: {count} files, {size} bytes"
        )
    return files


def digest(stream) -> bytes:
    sha = hashlib.sha256()
    while chunk := stream.read(1024 * 1024):
        sha.update(chunk)
    return sha.digest()


def package_zip() -> None:
    if TEMP.exists():
        raise SystemExit(f"Refusing to overwrite unfinished package: {TEMP}")
    members = [("RoadRash.bda", BDA), ("Rash/Rash.pak", PACK)]
    with zipfile.ZipFile(TEMP, "w", zipfile.ZIP_DEFLATED,
                         compresslevel=6, allowZip64=True) as archive:
        archive.writestr("INSTALL.txt", INSTALL.encode("utf-8"))
        for name, path in members:
            archive.write(path, name)
    with zipfile.ZipFile(TEMP, "r") as archive:
        expected = {name for name, _ in members} | {"INSTALL.txt"}
        if set(archive.namelist()) != expected:
            raise RuntimeError("ZIP member list mismatch")
        for name, path in members:
            if archive.getinfo(name).file_size != path.stat().st_size:
                raise RuntimeError(f"ZIP size mismatch: {name}")
            with archive.open(name) as packed, path.open("rb") as original:
                if digest(packed) != digest(original):
                    raise RuntimeError(f"ZIP content mismatch: {name}")
    # Keep the previous verified package intact until the new one passes every
    # member check, then swap it into place in a single filesystem operation.
    TEMP.replace(OUTPUT)
    report(OUTPUT, len(members) + 1)


def package_compact() -> None:
    if COMPACT_TEMP.exists():
        raise SystemExit(f"Refusing to overwrite unfinished package: {COMPACT_TEMP}")
    members = [("RoadRash.bda", BDA), ("Rash/Rash.pak", PACK)]
    with tarfile.open(COMPACT_TEMP, "w:xz", preset=6) as archive:
        import io
        install_data = INSTALL.encode("utf-8")
        info = tarfile.TarInfo("INSTALL.txt")
        info.size = len(install_data)
        archive.addfile(info, io.BytesIO(install_data))
        for name, path in members:
            archive.add(path, arcname=name, recursive=False)
    with tarfile.open(COMPACT_TEMP, "r:xz") as archive:
        expected = {name for name, _ in members} | {"INSTALL.txt"}
        if set(archive.getnames()) != expected:
            raise RuntimeError("Compact archive member list mismatch")
        for name, path in members:
            member = archive.getmember(name)
            if member.size != path.stat().st_size:
                raise RuntimeError(f"Compact archive size mismatch: {name}")
            with archive.extractfile(member) as packed, path.open("rb") as original:
                if digest(packed) != digest(original):
                    raise RuntimeError(f"Compact archive content mismatch: {name}")
    COMPACT_TEMP.replace(COMPACT_OUTPUT)
    report(COMPACT_OUTPUT, len(members) + 1)


def report(path: Path, count: int) -> None:
    with path.open("rb") as packed:
        checksum = digest(packed).hex()
    print(f"Package: {path}")
    print(f"Members: {count} (BDA, one pack of {EXPECTED_RESOURCE_COUNT} resources, INSTALL.txt)")
    print(f"Resource bytes: {PACK.stat().st_size}")
    print(f"Archive bytes: {path.stat().st_size}")
    print(f"SHA-256: {checksum}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compact", action="store_true",
                        help="create a smaller tar.xz package; extract on a PC before installing")
    args = parser.parse_args()
    files = source_files()
    if not BDA.is_file():
        raise SystemExit("Missing RoadRash.bda; build the game first")
    from build_resource_pack import build_pack
    build_pack(files)
    if args.compact:
        package_compact()
    else:
        package_zip()


if __name__ == "__main__":
    main()
