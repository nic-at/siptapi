This multiline verision of SIPTAPI is essentially an improved version
of the original Source-Forge SIPTAPI. It requires REFER support on the SIP side to
work properly - but it is stable and tested with various SIP phones
(SNOM, Eyebeam, Asterisk) and various TAPI application (Outlook,
dialer.exe, phoner). (Currently tested under Windows XP, Windows
Server 2003, Windows 7 and Windows 8 (32+64bit)

One major advantage of the multiline version is the use on
multiuser-systems (like Citrix installations). The GUI currently
supports configuration of 40 lines. Unlimited number of lines are
supported if the SIPTAPI configuration is done directly in the Windows
Registry, e.g. generate a .reg file and import the settings into the
Registry. For each user a dedicated line will be configured - these
lines are available globally to all users. Therefore, the user has to
choose its respective TAPI line in the TAPI application (e.g. Outlook).

This version also has support for incoming call indication.
This means, incoming calls (including caller-ID) will be signaled to
the application via the TAPI layer.

Note: SIPTAPI does not allow complete remote control of the
SIP phone. Thus, SIPTAPI does not support to answer or redirect/transfer
the incoming call. Also, SIPTAPI can not detect if an incoming call was
answered or canceled, or the duration of a phone call.

Note: Outlook does not support incoming call indication. Therefore,
if you want incoming call indication in Outlook you have to install
3rd party TAPI applications with Outlook support, e.g.
http://www.tapicall.com/ or http://www.identafone.com/poppro.html

Further, SIPTAPI is not a media provider and thus it
can not handle media at all. You still need a normal SIP phone to answer
the call. The incoming call indication is successfully tested with
Windows' dialer.exe (XP), Phoner, TAPIMaster Line Watcher and several
CRM applications.

The configuration GUI of SIPTAPI is limited to 40 lines because
more lines don't properly fit on the screen. But if you configure
SIPTAPI directly via the Windows Registry (instructions are provided)
there is no limit of concurrent lines (sometimes TAPI applications may
become slow if there are too many lines).

For incoming call indication SIPTAPI registers to the SIP server. If the
SIP server supports multiple registrations (e.g. Kamailio, FreeSwitch)
under the same SIP account then it should work out of the box. If not
(e.g. Asterisk), then you need to provision a dedicated SIP account for
SIPTAPI and configure the SIP server to ring both extensions on an incoming
call.
