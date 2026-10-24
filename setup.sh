#!/bin/bash
# The workspace of the OnePlus 9 Pro port: the two HackiOS forks, lemonadep branches, in hackios-xnu-kernel/.
#
# The kernel repository is cloned into hackios-xnu-kernel/, and its common/setup.sh then checks the userland
# repository out into the same directory (git dir hackios-xnu-kernel/.userland.git), as HackiOS is built.
# Run it again to fast-forward both.
#
#   KERNEL_URL=...    where to clone the kernel from (default: github.com/furangelz/hackios-xnu-kernel)
#   KERNEL_REF=...    its branch (default: lemonadep)
#   USERLAND_URL, USERLAND_REF: passed on to common/setup.sh (default there: the userland fork, lemonadep)
set -e
cd "$(dirname "$(readlink -f "$0")")"
URL=${KERNEL_URL:-https://github.com/furangelz/hackios-xnu-kernel.git}
REF=${KERNEL_REF:-lemonadep}

if [ ! -d hackios-xnu-kernel/.git ]; then
	echo "setup: cloning $URL ($REF) into hackios-xnu-kernel/"
	git clone --branch "$REF" "$URL" hackios-xnu-kernel
else
	echo "setup: hackios-xnu-kernel already here, updating"
	git -C hackios-xnu-kernel pull --ff-only || echo "setup: could not fast-forward hackios-xnu-kernel (local changes?)"
fi
exec hackios-xnu-kernel/common/setup.sh
