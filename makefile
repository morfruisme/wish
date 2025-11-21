# usage : make nix_user=<user> for nixos, else omit
all:
	if [[ -z "$(nix_user)" ]];\
	then gcc cmd.c main.c -o wish;\
	else gcc -DNIX_USER=\"$(nix_user)\" cmd.c main.c -o wish;\
	fi

clean:
	rm wish
