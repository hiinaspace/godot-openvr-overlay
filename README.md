# godot-openvr-overlay

This is a Godot extension for making 3D overlays on top of other SteamVR/OpenVR apps. Essentially it's Augmented Reality (AR) for VR.

![godot-openvr-overlay-demo.avif](demo scene running in front of Half Life Alyx)

It works by rendering a godot 3d scene into SteamVR/OpenVR's "projective"
overlays. It also exposes the playspace origin, headset/camera and controller
poses, so you can make overlays in a similar way to making a regular godot VR
game.

The repository includes a sample project in [project](./project). The default
scene shows a rotating cube and some debug text for each controller, plus a very
basic physics demo if you press the right trigger.

## Scene API

The extension currently registers these main node types:

* `OpenVROverlay`
* `OpenVROverlayOrigin3D`
* `OpenVROverlayCamera3D`
* `OpenVROverlayController3D`

The OpenVROverlay node is essentially the viewport for the overlay and the other nodes act essentially the same
as godot's [regular XR nodes](https://docs.godotengine.org/en/stable/tutorials/xr/index.html). Typical setup is:

* `OpenVROverlay` as the overlay root
* one `OpenVROverlayOrigin3D` child to define the tracking-space origin and world scale
* normal scene content parented under the overlay
* optional `OpenVROverlayCamera3D` / `OpenVROverlayController3D` nodes under the origin for tracked devices
* read controller inputs from the controller nodes
	* There is a openVR action map defined, but I honestly really don't understand the binding system so no idea
	  if it'll work for you.

## Requirements

* Godot `4.6+`
* SteamVR / OpenVR runtime, not OpenXR
  * SteamVR's OpenXR API doesn't expose a similar overlay like this unfortunately.
  * I don't think Oculus link/virtual desktop does either, so probably have to use steam link (I don't have a meta headset to test)
* Compatibility renderer (`gl_compatibility`), not Mobile/Forward+

## Download / Install

Prebuilt packages are attached to tagged releases:

https://github.com/hiinaspace/godot-openvr-overlay/releases/latest

Install into a Godot project by copying these files from the release zip into your project root:

* `bin/`
* `actions.json`
* `bindings_knuckles.json`

That should give you:

* `res://bin/godot-openvr-overlay.gdextension`
* `res://bin/windows/*.dll`
* `res://actions.json`
* `res://bindings_knuckles.json`

The included binaries are currently Windows `x86_64` only.

## Limitations

### Projection Accuracy

Godot does not expose arbitrary camera projection matrices through the stock
GDExtension API in a way this plugin can rely on for release builds, so the eye
camera projection currently uses `Camera3D::set_frustum()` as an approximation.
On the the Bigscreen Beyond 2 I tested this on, it was close enough in practice,
but still looks slightly "off".

Doing this exactly is probably blocked on [this Godot PR](https://github.com/godotengine/godot/pull/116424).

### Tracking Accuracy

The overlay will jitter around as you move your head more than the 'real' scene,
depending on your GPU/CPU load. This is because overlay applications do not get
the same timing and pose path as the main scene application, which normally
relies on compositor-driven frame pacing and pose prediction through
WaitGetPoses().

I don't think there's a good way to compensate for this, but if you know of one, please open an issue or PR.

## Wow I want to make an overlay but I don't like godot, what do

Lucky for you I also have a very minimal demo of projection overlays in

https://github.com/hiinaspace/openvr-overlay-bunny

It's python/OpenGL and fairly well documented; you can probably figure out how to do the same in another engine.
