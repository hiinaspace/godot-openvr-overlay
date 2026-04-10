# godot-openvr-overlay

This is a Godot extension for making 3D overlays on top of other SteamVR/OpenVR apps. Essentially it's Augmented Reality (AR) for VR.

![demo scene running in front of Half Life Alyx](godot-openvr-overlay-demo.avif)

It works by rendering a godot 3d scene into SteamVR/OpenVR's "projective"
overlays. It also exposes the playspace origin, headset/camera and controller
poses, so you can make overlays in a similar way to making a regular godot VR
game.

The repository includes a sample project in [project](./project). The default
scene shows a rotating cube and some debug text for each controller, plus a very
basic physics demo if you press the right trigger.

## How to Use

Download the [latest release from github](https://github.com/hiinaspace/godot-openvr-overlay/releases/latest)
and extract the zip into your project's `addons/` directory.

Then enable the `OpenVR Overlay` plugin in Project Settings.

Then follow [Godot's XR setup guide](https://docs.godotengine.org/en/stable/tutorials/xr/setting_up_xr.html), but change the interface:

```diff
-xr_interface = XRServer.find_interface("OpenXR")
+xr_interface = XRServer.find_interface("OpenVR Overlay")
```

Then when you press play, your app will display on top of any other OpenVR
apps running, instead of replacing them.

## Requirements

* Godot `4.6+`
* SteamVR / OpenVR runtime, not OpenXR
  * SteamVR's OpenXR API doesn't expose a similar overlay like this
    unfortunately (waiting warmly for `XR_EXTX_overlay`)
  * I don't think Oculus link/virtual desktop does either, so probably have to
    use steam link (I don't have a meta headset to test).
* Mobile or Forward+ Renderer
  * Compatability renderer could be supported without much effort but it doesn't
    at the moment.
* Windows x86_64
  * SteamVR/OpenVR theoreticaly works on Linux but not for me, so I
    didn't bother testing/building.

## Limitations

### Tracking Accuracy

The overlay will jitter around as you move your head more than the 'real' scene,
depending on your GPU/CPU load. This is because overlay applications do not get
the same timing and pose path as the main scene application, which normally
relies on compositor-driven frame pacing and pose prediction through
WaitGetPoses().

I don't think there's a good way to compensate for this, but if you know of one, please open an issue or PR.

### Right Eye Texture Blit

Godot renders both eyes in a single pass to a vulkan array texture. OpenVR's
projective overlays (apparently) don't support array textures. This extension
works around this by submitting the array texture to the left eye overlay as is (and steamvr reads layer 0/left eye), but has to blit the right eye to
a separate texture before submission.

## Wait how is this different than [godot_openvr](https://github.com/GodotVR/godot_openvr)?

This addon only supports making overlay apps, not regular OpenVR apps aka "Scene" applications. If you want to make a regular VR app in 2026, you should probably use OpenXR anyway, which is also built in to godot itself, instead of OpenVR.

In theory this extension could be merged into `godot_openvr`, but it's
kind of niche, so I haven't bothered trying. I think if you wanted to make a dual overlay/regular VR app, you may be better off still using OpenXR for the regular path and only add this extension just for the overlay mode.

## Wow I want to make an overlay but I don't like godot, what do?

Lucky for you I also have a very minimal demo of projection overlays in

https://github.com/hiinaspace/openvr-overlay-bunny

It's python/OpenGL and fairly well documented; you can probably figure out how to do the same in another engine.
