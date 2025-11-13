# Temporal API Design

This design hopes to layout an initial concept to adapting the `Temporal` API for Rust. The layout
was broadly mentioned in a previous PR earlier in the development of `Temporal`. This write up is to
mainly overview the API, and provide context to any potential contributors.

There are three incredibly important points to keep in mind during the design and implementation

1. This library should empower any language engine to implement `Temporal` to a high degree of fidelity.
2. This library should be able to provide a non-engine API consumable by any potential user.

Admittedly, as this library is being designed for use in a `JavaScript` engine. One takes priority,
but that should not completely overshadow the two.

The largest concerns come around the implementation for both `Calendar` and `TimeZone` (aka, `CalendarSlot`
and `TimeZoneSlot`).

Date is defined (broadly) as:

```rust

// In `Boa`, Date<C> is represented as Date<JsObject>
struct Date<C: CalendarProtocol> {
    iso: IsoDate,
    calendar: CalendarSlot<C>,
}

```

In this instance, C represents any Custom Calendar implementation that is required by the `Temporal`
specification. It is also fundamental to the design of the library; however, it introduces an
interesting API concern.

Primarily, what if someone doesn't want to implement a custom Calendar? Well, that's easy, we can just
use `Date<()>`. That's easy. We've solved the great API crisis!

But there's a catch. To provide utility to engine implementors, `CalendarProtocol` implements a `Context`
associated type. In order to have access to that context, it needs to be passed in as an argument.

So in an engine, like Boa, this may look like.

```rust
// Theoretically, fetching a the year value with context.
let year_value = Date<JsObject>.year(context);
```

But this API, makes the non-engine API just that much more cumbersome.

```rust
// A user fetching a year value.
let year_value = Date<()>.year(&mut ());
```

There is also the chance that some user WANTS to implement a custom calendar, but does NOT need any `Context`.

```rust
let year_value = Date<CustomCalendar>.year(&mut ());
```

The API in this instance is not necessarily bad, per se. It's just it could be better for non-engine consumers.

In order to address this issue, this design concept would require that any function that needs a `Context` argument
clearly labels itself as such.

```rust
let year_1 = Date<()>.year();
let year_2 = Date<()>.year_with_context(&mut ());
```
