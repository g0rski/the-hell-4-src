# DyniaMod

Manual multiplayer item sharing and Adria potion conversion based on the official The Hell 4 **v0.774**
source release of **October 13, 2026**. This workspace replaces the previous
March v0.208 source snapshot.

- [Official source download](https://thehellmod.ucoz.ru/Downloads/Src/th_team-th4-src_2026-10-13.zip)
- Source archive SHA-256: `8e59b657f558c6fa930bf20990dbd85273fd2b1c851314aecbeadf6d64636ba9`
- Executable: **DyniaMod v0.774.1**; separate multiplayer protocol revision **10774**.
- Save format: **30**, retaining the official 11,817-byte current player record.
- Only manual inventory drops are shared. Normal gameplay loot stays private.
- Adria's **Enchant Items** window also converts healing/mana potions for fixed
  fees, placing results in the backpack and refilling its input from the backpack
  or belt. See [conversion prices and validation](doc/potion-conversion.md).

See [the implementation and validation notes](doc/multiplayer-item-sharing.md)
and [Windows build instructions](BUILD.md). All multiplayer participants need
the same DyniaMod executable. Verification of the supplied v0.778 hero record
uses the production decoder and reader without editing the original save; full
Windows gameplay verification is still required.

The unmodified author readme is included as [__README.txt](__README.txt) and
reproduced below. Include it when redistributing this source. This derivative
uses a different name as required by the author.

## Original author readme

This is Readme file to accompany The Hell 4 source v0.774.
Dated October 13, 2026.

You are free to do as you wish with this source. Create derivative works or whatever else. Just give them different name. Not "The Hell".
At the time of writing this readme AI models are growing fast and I expect people will use them more and more to explore creative boundaries.
Compared to previous source I released in March 2026, this one should be more convenient to use for making submods.
Also, all of old russian commentaries in source were translated to english.

You can redistribute the source but with this Readme included.

In its core it is Hellfire v1.01 that was decompiled with IDA long time ago (it began in 2007).
It's not completely cleaned out, some of that decompilatus Diablii is still there but it's mostly functions that are not really in the need of edits.

How to install the source:
1. InitProject.bat
2. After this, edit personal.props (set the directory for the mod)

I don't include my workshop along with this source as it is several gigs in size. And I don't want to publish it just yet.

There is "modding" channel in The Hell 4 discord chat, where we can discuss everything related to making mods and programming.
Here is link to this discord: https://discord.gg/AN9NukZFbN

Have fun and good luck with whatever you're up to.

Note: this readme may receive updates in future if there is need for that.

Mordor
