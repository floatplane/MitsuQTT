#!/usr/bin/env ruby

require 'webrick'
require 'mustache'

root = File.expand_path '~/public_html'
server = WEBrick::HTTPServer.new :Port => 8000, :DocumentRoot => root

class Template < Mustache
    def partial(name)
        begin
            path = case name.to_s
            when "mqttTextField" 
                File.expand_path("_text_field.mst", "src/frontend/en-us/views/mqtt")
            else 
                File.expand_path("#{name}.mst", "src/frontend/en-us/partials")
            end
            # puts "loading partial '#{name}' from file '#{path}'"
            File.read path
        rescue
            puts "partial not found #{name}: tried #{path}"
            raise if raise_on_context_miss?
            ""
        end
    end

    def render_with_default_context(path, context)
        default_context = {
            header: {
                hostname: "the_hostname",
                git_hash: "89abcdef",
            },
            footer: {
                version: "2001.01.01",
                git_hash: "89abcdef",
            },
            hostname: "the_hostname",
            showControl: true,
            showLogout: true,
            SSID: "the_ssid",
        }
        template = File.expand_path("#{path}.mst", "src/frontend/en-us/views")
        self.class.render(File.read(template), default_context.merge(context))
    end
end

def render(path, context = {})
    Template.new.render_with_default_context path, context
end

server.mount_proc '/' do |req, res|
    # puts req.path
    case req.path
    when "/css" then res.body = File.read File.expand_path("mvp.css", "src/frontend/statics")
    
    when "/captive/" then res.body = render("captive/index", {hostname: "the_hostname"})
    when "/captive/reboot" then res.body = render("captive/reboot")
    when "/captive/save" then res.body = render("captive/save", {access_point: "the_ssid", hostname: "the_hostname"})
    
    when "/mqtt" then res.body = render("mqtt/index", {
        hostname: "the_hostname",
        friendlyName: {
            label: "Friendly name",
            value: "the_friendly_name",
            param: "fn"
        },
        server: {
            label: "MQTT server",
            value: "mqtt.example.com",
            param: "mh"
        },
        port: {
            value: "1883",
        },
        password: {
            value: "abc123",
        },
        user: {
            value: "the_username",
            label: "Username",
            param: "mu",
            placeholder: "mqtt_user",
        },
        topic: {
            value: "the_topic",
            label: "Topic",
            param: "mt",
            placeholder: "topic",
        },
    })

    when "/" then res.body = render("index")
    when "/control" then res.body = render("control", {
        min_temp: "16",
        current_temp: "20.1",
        target_temp: "22",
        max_temp: "30",
        temp_step: "0.5",
        temp_unit: "C",
        supportHeatMode: false,
        power: true,
        mode: {
            cool: true,
        },
        fan: {
            "3": true,
        },
        vane: {
            auto: true,
        },
        widevane: {
            "1": true,
        },
    })
    when "/others" then res.body = render("others", {
        topic: "the_topic",
        dumpPacketsToMqtt: true,
        logToMqtt: true,
        toggles: [
            {title: "Safe mode", name: "SafeMode", value: true},
            {title: "Optimistic updates", name: "OptimisticUpdates", value: true},
            {title: "MQTT topic debug logs", name: "DebugLogs", value: true},
            {title: "MQTT topic debug packets", name: "DebugPckts", value: true},
        ],
    })
    when "/reboot" then res.body = render("reboot", {saving: true})
    when "/reset" then res.body = render("reset", {SSID: "the_ssid"})
    when "/setup" then res.body = render("setup")
    when "/status" then res.body = render("status", {
        uptime: {
            years: 1,
            days: 2,
            hours: 3,
            minutes: 4,
            seconds: "5.060",
        },
        hvac_connected: true,
        hvac_retries: "0",
        mqtt_connected: false,
        mqtt_error_code: 0,
        wifi_access_point: "the_ssid",
        wifi_signal_strength: "-66",
    })
    when "/unit" then res.body = render("unit", {
        min_temp: "16",
        max_temp: "30",
        temp_step: "0.5",
        temp_unit_c: true,
        mode_selection_all: true,
        login_password: "",
    })
    when "/unit_alt" then res.body = render("unit", {
        min_temp: "55",
        max_temp: "72",
        temp_step: "1",
        temp_unit_c: false,
        mode_selection_all: false,
        login_password: "abc123",
    })
    when "/upgrade" then res.body = render("upgrade")
    when "/upload" then res.body = render("upload")
    when "/wifi" then res.body = render("wifi", {access_point: "the_ssid", hostname: "the_hostname", password: "abc123"})
        
    else
        res.body = "not found"
        res.status = 404
    end
end

trap 'INT' do server.shutdown end

server.start