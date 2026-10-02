xmakeで自動ビルドできるようにしたいです。
これを参考にしてください。https://github.com/SARDONYX-sard/fnis_aa/blob/main/cxx/xmake.lua

- 人間が見やすく分離してください
  1. 変数名は1語か2語までに短くしてください。例）valueCheck
  2. 関数は責務を分割してください。
  3. 1ファイル500行以内になるようにファイル分割してください。
  4. 適切な改行とコメントをrustのような英語ドキュメントで記載してください。例）https://github.com/SARDONYX-sard/fnis_aa/blob/main/cxx/src/menu.cc#L86-L90

- cmakeをxmakeにしてください。ref (https://github.com/SARDONYX-sard/fnis_aa/blob/main/cxx/xmake.lua)
- depsはexternalではなく(CommonLibSSENGはs)

- また、FreeClimb.xmlを読んで、espなしでdllを生成できるようにしてください。

---

-
