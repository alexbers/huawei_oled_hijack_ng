#!/bin/sh

SMS_WEBHOOK_CLIENT="/app/hijack/bin/sms_webhook_client"
LUARUN="/system/xbin/luarun"

FORMAT_ALL_SMS='
dm = require("dm")
sys = require("sys")
xml = require("xml")


function print_r (t)
    local print_r_cache={}
    local function sub_print_r(t,indent)
        if (print_r_cache[tostring(t)]) then
            print(indent.."*"..tostring(t))
        else
            print_r_cache[tostring(t)]=true
            if (type(t)=="table") then
                for pos,val in pairs(t) do
                    if (type(val)=="table") then
                        print(indent.."["..pos.."] => "..tostring(t).." {")
                        sub_print_r(val,indent..string.rep(" ",string.len(pos)+8))
                        print(indent..string.rep(" ",string.len(pos)+6).."}")
                    else
                        print(indent.."["..pos.."] => "..tostring(val))
                    end
                end
            else
                print(indent..tostring(t))
            end
        end
    end
    sub_print_r(t,"  ")
end

function format_sms_read_menu(sms_count)
    sms_count = 500
    print(sms_count)
    for i = 0,sms_count-1,15 do
        print("item:"..(i+1).."-"..math.min(i+15,sms_count)..":SMS_ALL_PAGE_"..(math.floor(i/15)+1))
    end
end

function format_sms_page(page, content, webhook_client, prefer_unread)
    local messages = xml.decode(content)["response"]["Messages"]
    local count = tonumber(xml.decode(content)["response"]["Count"])

    if (count == 0) then
        print("text:That was the last SMS")
        return
    end

    message = messages["Message"]

    if prefer_unread == 1 and tonumber(message.Smstat) == 1 then
        print("text:No more unreaded SMS")
        return
    end

    req = "<request><Index>"..message.Index.."</Index></request>"
    cmd = webhook_client.." sms set-read 2 \""..req.."\""
    sys.exec(cmd.." > /dev/null")

    print("text:"..message.Phone)
    print("text:"..message.Date)
    print("text:"..message.Content)

    if prefer_unread == 1 then
        print("item:<Next>:SMS_UNREAD_READ "..(page))
    else
        print("item:<Next>:SMS_ALL_READ "..(page+1))
    end
    print("item:<Delete>:SMS_DELETE "..message.Index.." "..page.." "..prefer_unread)

end

'

get_unread_mesages_count () {
    "$SMS_WEBHOOK_CLIENT" sms sms-count 1 0 | grep LocalUnread | grep -Eo '[0-9]+'
}

get_mesages_count () {
    "$SMS_WEBHOOK_CLIENT" sms sms-count 1 0 | grep LocalInbox | grep -Eo '[0-9]+'
}

remove_newlines () {
    echo "${1//
    /}"
}

format_sms() {
    local PAGE="$1"
    local PREFER_UNREAD="$2"
    local REQUEST="
        <request>
            <PageIndex>$PAGE</PageIndex>
            <ReadCount>1</ReadCount>
            <BoxType>1</BoxType>
            <SortType>0</SortType>
            <Ascending>0</Ascending>
            <UnreadPreferred>$PREFER_UNREAD</UnreadPreferred>
        </request>
    "
    REQUEST="$(remove_newlines "$REQUEST")"

    local XML="$("$SMS_WEBHOOK_CLIENT" sms sms-list 2 "$REQUEST")"
    echo "$FORMAT_ALL_SMS format_sms_page(Argv[1], Argv[2], Argv[3], $PREFER_UNREAD)" | \
         "${LUARUN}" "$PAGE" "$XML" "$SMS_WEBHOOK_CLIENT"
}

if [ "$#" -eq 0 ]; then
    echo "text:SMS:"
    echo "item:New ($(get_unread_mesages_count)):SMS_UNREAD_READ 1"
    echo "item:All ($(get_mesages_count)):SMS_ALL_READ 1"
elif [ "$#" -eq 1 ]; then
    case "$1" in
        SMS_ALL )
            echo "$FORMAT_ALL_SMS format_sms_read_menu(Argv[1])" | "${LUARUN}" "$(get_mesages_count)"
            ;;
        * )
            echo "text: wrong command mode"
            exit 1
            ;;
    esac

elif [ "$#" -eq 2 ]; then
    case "$1" in
        SMS_ALL_READ )
            PAGE="$2"
            format_sms "$PAGE" 0
            ;;
        SMS_UNREAD_READ )
            PAGE="$2"
            format_sms "$PAGE" 1
            ;;
        * )
            echo "text: wrong command mode"
            exit 1
            ;;
    esac
elif [ "$#" -eq 4 ]; then
    case "$1" in
        SMS_DELETE )
            ID="$2"
            PAGE="$3"
            PREFER_UNREAD="$4"

            "$("$SMS_WEBHOOK_CLIENT" sms delete-sms 2 "<request><Index>${ID}</Index></request>")"

            format_sms "$PAGE" "$PREFER_UNREAD"
            ;;
        * )
            echo "text: wrong command mode"
            exit 1
            ;;
    esac
fi
