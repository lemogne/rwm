#!/usr/bin/sh

# WIP

if [ "$(id -u)" -ne 0 ]; then
	echo "Please run as root" >&2
	exit 1
fi

if [ ! -f rwm ]; then
	scripts/build.sh
fi

# Straightforward: user is unlikely to have these files installed
cp rwm bin/rwmdebug bin/rwnopen /usr/bin
mkdir -p "$HOME/.config/rwm"
cp etc/*.cfg "$HOME/.config/rwm"


# Convoluted: we need to check if these files already exist, ans if so, adjust where we install them

if [ ! -f /usr/bin/xdg-mime ]; then
	# This is fine since our xdg-mime is unmodified
	cp bin/xdg-mime /usr/bin/xdg-mime
fi

# We shouldn't install our xdg-open to /usr/bin since even if the user does not have it installed via a graphical DE,
# he may want to do so later, which would either conflict with or replace our version, which will break things
mkdir -p /usr/bin/rwmbin
cp xdg-open /usr/bin/rwmbin/xdg-open

# Similar considerations for mime databases

# ...