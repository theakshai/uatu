# Uatu: Directory Time Tracker Integration

# Set the path to the uatu binary
if (( $+commands[uatu] )); then
    UATU_BIN=${commands[uatu]}
elif [[ -x "$HOME/.local/bin/uatu" ]]; then
    UATU_BIN="$HOME/.local/bin/uatu"
elif [[ -x "$HOME/bin/uatu" ]]; then
    UATU_BIN="$HOME/bin/uatu"
else
    # Fallback to current directory for development
    UATU_BIN="./uatu"
fi

# Check if binary exists
if [[ ! -x "$UATU_BIN" ]]; then
    # Silently fail or warn? Let's warn once.
    echo "Warning: Uatu binary not found at $UATU_BIN. Tracking disabled."
    return
fi

# Ensure hooks are available
autoload -U add-zsh-hook

# Function to start tracking a directory
_uatu_chpwd() {
    # Stop previous session if it exists (optional, but good for cleanliness)
    if [[ -n "$UATU_SESSION_ID" ]]; then
        "$UATU_BIN" stop "$UATU_SESSION_ID" > /dev/null 2>&1
    fi
    
    # Start new session
    # We capture the ID. If it fails, ID is empty.
    local new_id
    new_id=$("$UATU_BIN" start "$PWD")
    
    if [[ -n "$new_id" ]]; then
        typeset -g UATU_SESSION_ID="$new_id"
    else
        unset UATU_SESSION_ID
    fi
}

# Heartbeat function
_uatu_heartbeat() {
    if [[ -n "$UATU_SESSION_ID" ]]; then
        local new_id
        # Heartbeat now returns the current valid session ID
        new_id=$("$UATU_BIN" heartbeat "$UATU_SESSION_ID" 2>/dev/null)
        
        # If heartbeat succeeded and returned a valid integer ID, update our tracking variable
        if [[ "$new_id" =~ ^[0-9]+$ ]]; then
            typeset -g UATU_SESSION_ID="$new_id"
        else
            # If heartbeat failed (e.g. session deleted or DB error), we might want to unset
            # OR we might want to keep the old ID and retry?
            # If the session is invalid in DB, we should probably stop tracking it.
            unset UATU_SESSION_ID
        fi
    fi
}

# Stop tracking on shell exit
_uatu_stop_on_exit() {
    if [[ -n "$UATU_SESSION_ID" ]]; then
        "$UATU_BIN" stop "$UATU_SESSION_ID" > /dev/null 2>&1
        unset UATU_SESSION_ID
    fi
}

# Hook into directory change
add-zsh-hook chpwd _uatu_chpwd

# Hook into prompt (runs before prompt is redisplayed)
add-zsh-hook precmd _uatu_heartbeat

# Hook into command execution (runs before a command is executed)
add-zsh-hook preexec _uatu_heartbeat

# Hook into shell exit
add-zsh-hook zshexit _uatu_stop_on_exit

# Initial setup for the shell start
# Manually trigger start for the current directory
_uatu_chpwd
