#pragma once

#include <cstdint>

namespace discusy {

enum class Opcode : std::uint8_t {
    /// An event was dispatched.
    Dispatch = 0,
    /// Fired periodically by the client to keep the connection alive.
    Heartbeat = 1,
    /// Starts a new session during the initial handshake.
    Identify = 2,
    /// Update the client’s presence.
    PresenceUpdate = 3,
    /// Used to join/leave or move between voice channels.
    VoiceStateUpdate = 4,
    /// Resume a previous session that was disconnected.
    Resume = 6,
    /// You should attempt to reconnect and resume immediately.
    Reconnect = 7,
    /// Request information about offline guild members in a large guild.
    RequestGuildMembers = 8,
    /// The session has been invalidated. You should reconnect and identify/resume accordingly.
    InvalidSession = 9,
    /// Sent immediately after connecting, contains the heartbeat_interval to use.
    Hello = 10,
    /// Sent in response to receiving a heartbeat to acknowledge that it has been received.
    HeartbeatAck = 11,
    /// Request information about soundboard sounds in a set of guilds.
    RequestSoundboardSounds = 31,
    /// Request ephemeral channel data for channels in a guild.
    RequestChannelInfo = 43,
};

enum class CloseCode : std::uint16_t {
    /// We’re not sure what went wrong. Try reconnecting?
    UnknownError = 4000,
    /// You sent an invalid Gateway Opcode or an invalid payload for an Opcode. Don’t do that!
    UnknownOpcode = 4001,
    /// You sent an invalid payload to Discord. Don’t do that!
    DecodeError = 4002,
    /// You sent us a payload prior to identifying, or this session has been invalidated.
    NotAuthenticated = 4003,
    /// The account token sent with your identify payload is incorrect.
    AuthenticationFailed = 4004,
    /// You sent more than one identify payload. Don’t do that!
    AlreadyAuthenticated = 4005,
    /// The sequence sent when resuming the session was invalid. Reconnect and start a new session.
    InvalidSeq = 4007,
    /// Woah nelly! You’re sending payloads to us too quickly. Slow it down! You will be disconnected on receiving this.
    RateLimited = 4008,
    /// Your session timed out. Reconnect and start a new one.
    SessionTimedOut = 4009,
    /// You sent us an invalid shard when identifying.
    InvalidShard = 4010,
    /// The session would have handled too many guilds - you are required to shard your connection in order to connect.
    ShardingRequired = 4011,
    /// You sent an invalid version for the gateway.
    InvalidApiVersion = 4012,
    /// You sent an invalid intent for a Gateway Intent. You may have incorrectly calculated the bitwise value.
    InvalidIntents = 4013,
    /// You sent a disallowed intent for a Gateway Intent. You may have tried to specify an intent that you have not enabled or are not approved for.
    DisallowedIntents = 4014,
};

enum class VoiceOpcode : std::uint8_t {
    /// Begin a voice websocket connection.
    Identify = 0,
    /// Select the voice protocol.
    SelectProtocol = 1,
    /// Complete the websocket handshake.
    Ready = 2,
    /// Keep the websocket connection alive.
    Heartbeat = 3,
    /// Describe the session.
    SessionDescription = 4,
    /// Indicate which users are speaking.
    Speaking = 5,
    /// Sent to acknowledge a received client heartbeat.
    HeartbeatAck = 6,
    /// Resume a connection.
    Resume = 7,
    /// Time to wait between sending heartbeats in milliseconds.
    Hello = 8,
    /// Acknowledge a successful session resume.
    Resumed = 9,
    /// One or more clients have connected to the voice channel.
    ClientsConnect = 11,
    /// A client has disconnected from the voice channel.
    ClientDisconnect = 13,
    /// A downgrade from the DAVE protocol is upcoming.
    DavePrepareTransition = 21,
    /// Execute a previously announced protocol transition.
    DaveExecuteTransition = 22,
    /// Acknowledge readiness previously announced transition.
    DaveTransitionReady = 23,
    /// A DAVE protocol version or group change is upcoming.
    DavePrepareEpoch = 24,
    /// Credential and public key for MLS external sender.
    DaveMlsExternalSender = 25,
    /// MLS Key Package for pending group member.
    DaveMlsKeyPackage = 26,
    /// MLS Proposals to be appended or revoked.
    DaveMlsProposals = 27,
    /// MLS Commit with optional MLS Welcome messages.
    DaveMlsCommitWelcome = 28,
    /// MLS Commit to be processed for upcoming transition.
    DaveMlsAnnounceCommitTransition = 29,
    /// MLS Welcome to group for upcoming transition.
    DaveMlsWelcome = 30,
    /// Flag invalid commit or welcome, request re-add.
    DaveMlsInvalidCommitWelcome = 31,
};

enum class VoiceCloseCode : std::uint16_t {
    /// You sent an invalid opcode.
    UnknownOpcode = 4001,
    /// You sent an invalid payload in your identifying to the Gateway.
    FailedToDecodePayload = 4002,
    /// You sent a payload before identifying with the Gateway.
    NotAuthenticated = 4003,
    /// The token you sent in your identify payload is incorrect.
    AuthenticationFailed = 4004,
    /// You sent more than one identify payload. Stahp.
    AlreadyAuthenticated = 4005,
    /// Your session is no longer valid.
    SessionNoLongerValid = 4006,
    /// Your session has timed out.
    SessionTimeout = 4009,
    /// We can’t find the server you’re trying to connect to.
    ServerNotFound = 4011,
    /// We didn’t recognize the protocol you sent.
    UnknownProtocol = 4012,
    /// Disconnect individual client (you were kicked, the main gateway session was dropped, etc.). Should not reconnect.
    Disconnected = 4014,
    /// The server crashed. Our bad! Try resuming.
    VoiceServerCrashed = 4015,
    /// We didn’t recognize your encryption.
    UnknownEncryptionMode = 4016,
    /// This channel requires a client supporting E2EE via the DAVE Protocol.
    E2eeDaveProtocolRequired = 4017,
    /// You sent a malformed request.
    BadRequest = 4020,
    /// Disconnect due to rate limit exceeded. Should not reconnect.
    DisconnectedRateLimited = 4021,
    /// Disconnect all clients due to call terminated (channel deleted, voice server changed, etc.). Should not reconnect.
    DisconnectedCallTerminated = 4022,
};


}