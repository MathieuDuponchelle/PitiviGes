# Introduction

> You may sometimes find some language-agnostic notes here as well

```c
/*
 * Here you will find all the examples, as well as the function prototypes in
 * C. You may want to click the "Python" or the shell tab now.
 */
```

```python
# Here you go! Python is obviously the best choice, forget these other tabs!
```

```shell
# Not all examples translate to shell, but you can emulate most of them thanks
# to ges-launch-1.0!
```

Welcome to the Gstreamer-Editing-Services (GES) documentation.

This is the API used under the hood by [Pitivi](http://www.pitivi.org/), so
anything possible in Pitivi's UI can be done with GES as well.

This document's objective is to serve both as as an API reference and a
step-by-step tutorial / guide. We will start with the basic concepts,
illustrating them with examples in various languages, and by the end you
should be able to write a non-linear video editing software for the platform
of your choice.

A search box is available on the left-hand side for quick lookup of classes or
methods, examples and prototypes can be found in the right-hand side column.

All the examples are available in the [examples directory of
GES](http://cgit.freedesktop.org/gstreamer/gst-editing-services/tree/tests/examples),
during the course of this tutorial we will create specialized functions that we
will reuse in latter examples, these functions will be cross-referenced all
along the document, so you don't need to follow the tutorial in a strictly
sequential order, even though it is recommended to get a complete understanding
of the API.

### About GES

Gstreamer-Editing-Services (GES) is a C library providing a high-level API for
multimedia editing, it is based on
[GStreamer](http://gstreamer.freedesktop.org/), a very powerful, versatile and
portable multimedia framework.

GES is Open Source, licensed under the [LGPL
v2+](http://www.gnu.org/licenses/lgpl-2.0.html) or (at your option) any later version.
You are encouraged to look at the source code and contribute to its development by
submitting patches to our [phabricator instance](http://phabricator.freedesktop.org/)!

GES can be deployed on iOS, Windows, Linux, Android and OSX.

### Features

GES lets you edit and compose together any number of multimedia resources,
apply filters on them and animate their properties.

The range of tasks GES can perform is very wide, from simple three-point
editing (playing or rendering a source clip from an in-point to an out-point),
to complex timelines with hundreds of source clips composited together
on multiple layers, filters with animatable properties and many
different types of transitions.

GES provides you with an easy-to-use rendering and playing back abstraction,
and an executable, ges-launch-1.0, which allows access to a significant
part of its features through the command-line, ideal for quick prototyping
and testing.

Finally, GES also exposes a lower-level API, for the adventurous developer who
wishes to implement new kinds of sources, effects or transitions.
