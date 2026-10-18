## Contribution Guidelines

- Issues and PRs must be written by a human.

- Your commit message must include the following in the trailer if you used AI or an LLM.

  ``Assisted-by: LLM`` if it was involved in making
  decisions or also generated parts of the code.

  ``Generated-by: LLM`` if almost all the code was
  generated through it.

- Your code should follow Intel's modified Linux coding style.

  Use `astyle --style=linux -cnpUH -s4 -M120 <file>` or `./style_unify` to automate this.

- Your commits should do one thing at a time.

  Exemptions are made for very large change-sets, `+200,000` lines or higher for example.

- Avoid making commits that fix up previous commits in your PR.

  `git commit --fixup` along with auto-squash is your best-friend.

## New to Git?

https://chris.beams.io/git-commit is a pretty neat guide for how to write good commits.

We encourage the use of `Fixes: <commit hash>` and `Closes: <url to the GitHub issue>` in your commit messages.
