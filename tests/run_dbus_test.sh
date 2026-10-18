#!/usr/bin/env bash
# Invoked through setsid: every D-Bus activation child belongs to this private group.
set -u
if [[ $# -lt 3 ]]; then
    printf 'Usage: run_dbus_test.sh TIMEOUT DBUS_RUN_SESSION EXECUTABLE [ARGS...]\n' >&2
    exit 2
fi
pulse_timeout="$1"
pulse_dbus="$2"
shift 2
pulse_group="$$"
pulse_members=()

collect_group_members() {
    pulse_members=()
    local stat_file stat_line pid fields state parent group unused
    for stat_file in /proc/[0-9]*/stat; do
        [[ -r "$stat_file" ]] || continue
        IFS= read -r stat_line < "$stat_file" 2>/dev/null || continue
        pid="${stat_file#/proc/}"
        pid="${pid%/stat}"
        [[ "$pid" != "$$" ]] || continue
        # The comm field may contain spaces and parentheses; numeric fields follow ') '.
        fields="${stat_line##*) }"
        read -r state parent group unused <<< "$fields"
        [[ "$group" = "$pulse_group" && "$state" != Z ]] && pulse_members+=("$pid")
    done
}

cleanup_group() {
    # The wrapper survives its own group TERM and returns the tested exit status.
    trap '' TERM INT HUP
    kill -TERM -- "-$pulse_group" 2>/dev/null || true
    local deadline=$((SECONDS + 2))
    while ((SECONDS < deadline)); do
        collect_group_members
        ((${#pulse_members[@]})) || return 0
        sleep 0.05
    done
    # Kill only remaining members verified in our private group, never the caller.
    collect_group_members
    if ((${#pulse_members[@]})); then
        kill -KILL -- "${pulse_members[@]}" 2>/dev/null || true
    fi
    wait 2>/dev/null || true
}
trap cleanup_group EXIT
trap 'exit 143' TERM HUP
trap 'exit 130' INT

# --foreground keeps D-Bus activation descendants in this wrapper's private group.
timeout --foreground --signal=TERM --kill-after=2s "$pulse_timeout" "$pulse_dbus" -- "$@" &
pulse_child="$!"
wait "$pulse_child"
pulse_result="$?"
trap - EXIT
cleanup_group
exit "$pulse_result"
