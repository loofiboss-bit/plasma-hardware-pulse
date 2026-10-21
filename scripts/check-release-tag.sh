#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.."
[[ ${GITHUB_REF:-} == refs/tags/* ]] || { echo 'A tag ref is required.' >&2; exit 1; }
[[ ${GITHUB_REF_NAME:-} == "v$(cat VERSION)" ]] || { echo 'Release tag does not match VERSION.' >&2; exit 1; }
git -c safe.directory="$PWD" rev-parse --verify refs/remotes/origin/main >/dev/null
git -c safe.directory="$PWD" merge-base --is-ancestor HEAD refs/remotes/origin/main
[[ $(git -c safe.directory="$PWD" rev-parse HEAD) == "$(git -c safe.directory="$PWD" rev-parse "refs/tags/$GITHUB_REF_NAME^{commit}")" ]]
echo 'Release tag matches VERSION and is reachable from origin/main.'
