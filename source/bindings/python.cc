#include "yellowstone/yellowstone.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace py = pybind11;

namespace
{
  using PyObjectPtr = std::shared_ptr<PyObject>;

  [[nodiscard]] PyObjectPtr hold_python_object(py::handle value)
  {
    PyObject* object = value.ptr();
    Py_INCREF(object);

    return PyObjectPtr(
      object,
      [](PyObject* owned)
      {
        py::gil_scoped_acquire gil{};
        Py_DECREF(owned);
      }
    );
  }

  [[nodiscard]] py::object borrow_python_object(const PyObjectPtr& value)
  {
    return py::reinterpret_borrow<py::object>(value.get());
  }

  template <typename Bound>
  [[nodiscard]] py::object make_generic_alias(py::object parameter)
  {
    return py::module_::import("types").attr("GenericAlias")(
      py::type::of<Bound>(),
      std::move(parameter)
    );
  }

  struct PythonPayload final
  {
    PyObjectPtr object{};
  };

  struct PythonEvent final
  {
    yellowstone::EventID id{0UL};
    yellowstone::Superstep superstep{0UL};
    yellowstone::Sequence sequence{0UL};
    PyObjectPtr payload{};
  };

  class PythonScheduler;

  class PythonEventBus final
  {
  public:
    using NativeBus = yellowstone::EventBus<PythonPayload>;
    using NativeEvent = NativeBus::EventType;

    PythonEventBus(
      py::object payload_type,
      const std::size_t capacity,
      py::object merge
    )
      : m_payload_type(m_hold_payload_type(payload_type)),
        m_bus(
          capacity,
          m_make_merger(std::move(merge))
        )
    {
    }

    PythonEventBus(const PythonEventBus&) = delete;
    PythonEventBus(PythonEventBus&&) = delete;
    void operator=(const PythonEventBus&) = delete;
    void operator=(PythonEventBus&&) = delete;

    ~PythonEventBus() noexcept
    {
      m_bus.stop();
    }

    [[nodiscard]] yellowstone::SubscriberID subscribe(py::function handler)
    {
      auto callback = hold_python_object(handler);

      return m_bus.subscribe(
        [callback](const NativeEvent& event)
        {
          py::gil_scoped_acquire gil{};
          PythonEvent snapshot{
            event.id(),
            event.superstep(),
            event.sequence(),
            event.payload().object,
          };

          try
          {
            auto callable = py::reinterpret_borrow<py::function>(callback.get());
            callable(py::cast(snapshot));
          }
          catch (py::error_already_set& error)
          {
            error.discard_as_unraisable("yellowstone subscriber");
          }
        }
      );
    }

    void unsubscribe(const yellowstone::SubscriberID subscriber_id)
    {
      m_bus.unsubscribe(subscriber_id);
    }

    [[nodiscard]] yellowstone::PublishResult publish(
      const yellowstone::EventID event_id,
      py::object payload,
      const yellowstone::Superstep superstep
    )
    {
      m_validate_payload(payload);
      return m_bus.publish(
        event_id,
        PythonPayload{hold_python_object(payload)},
        superstep
      );
    }

    [[nodiscard]] yellowstone::PublishResult publish_one(
      const yellowstone::SubscriberID subscriber_id,
      const yellowstone::EventID event_id,
      py::object payload,
      const yellowstone::Superstep superstep
    )
    {
      m_validate_payload(payload);
      return m_bus.publish_one(
        subscriber_id,
        event_id,
        PythonPayload{hold_python_object(payload)},
        superstep
      );
    }

    void start(void)
    {
      m_bus.start();
    }

    void stop(void) noexcept
    {
      m_bus.stop();
    }

    [[nodiscard]] bool running(void) const noexcept
    {
      return m_bus.running();
    }

    [[nodiscard]] std::size_t subscriber_count(void) const noexcept
    {
      return m_bus.subscriber_count();
    }

    [[nodiscard]] std::size_t pending(const yellowstone::SubscriberID id) const
    {
      return m_bus.pending(id);
    }

    [[nodiscard]] std::size_t capacity(void) const noexcept
    {
      return m_bus.partition_capacity();
    }

    [[nodiscard]] py::object payload_type(void) const
    {
      return borrow_python_object(m_payload_type);
    }

  private:
    friend class PythonScheduler;

    [[nodiscard]] static PyObjectPtr m_hold_payload_type(py::handle payload_type)
    {
      if (!PyType_Check(payload_type.ptr()))
      {
        throw py::type_error("payload_type must be a Python type");
      }
      return hold_python_object(payload_type);
    }

    [[nodiscard]] NativeBus::Merger m_make_merger(py::object merge)
    {
      if (merge.is_none())
      {
        return {};
      }

      if (!PyCallable_Check(merge.ptr()))
      {
        throw py::type_error("merge must be callable or None");
      }

      auto callback = hold_python_object(merge);
      auto expected_type = m_payload_type;

      return [callback, expected_type](PythonPayload& current, const PythonPayload& incoming)
      {
        py::gil_scoped_acquire gil{};
        auto callable = py::reinterpret_borrow<py::function>(callback.get());
        py::object result = callable(
          borrow_python_object(current.object),
          borrow_python_object(incoming.object)
        );

        const int compatible = PyObject_IsInstance(result.ptr(), expected_type.get());
        if (compatible < 0)
        {
          throw py::error_already_set();
        }
        if (0 == compatible)
        {
          throw py::type_error("merge returned a value incompatible with payload_type");
        }

        current.object = hold_python_object(result);
      };
    }

    void m_validate_payload(py::handle payload) const
    {
      const int compatible = PyObject_IsInstance(payload.ptr(), m_payload_type.get());
      if (compatible < 0)
      {
        throw py::error_already_set();
      }
      if (0 == compatible)
      {
        throw py::type_error("event payload is incompatible with this EventBus payload_type");
      }
    }

    PyObjectPtr m_payload_type;
    NativeBus m_bus;
  };


  class PythonScheduler final
  {
  public:
    using NativeScheduler = yellowstone::Scheduler<PythonPayload>;

    PythonScheduler(
      PythonEventBus& event_bus,
      const std::int64_t retry_delay_nanoseconds
    )
      : m_event_bus(event_bus),
        m_scheduler(
          event_bus.m_bus,
          std::chrono::nanoseconds{retry_delay_nanoseconds},
          &PythonScheduler::m_report_error
        )
    {
    }

    PythonScheduler(const PythonScheduler&) = delete;
    PythonScheduler(PythonScheduler&&) = delete;
    void operator=(const PythonScheduler&) = delete;
    void operator=(PythonScheduler&&) = delete;

    ~PythonScheduler() noexcept = default;

    [[nodiscard]] yellowstone::ScheduleID schedule(
      const yellowstone::EventID event_id,
      py::object payload,
      const std::int64_t forward_at_unix_nanoseconds,
      const yellowstone::Superstep superstep
    )
    {
      m_event_bus.m_validate_payload(payload);

      return m_scheduler.schedule_at_unix_nanoseconds(
        event_id,
        PythonPayload{hold_python_object(payload)},
        forward_at_unix_nanoseconds,
        superstep
      );
    }

    [[nodiscard]] yellowstone::ScheduleID schedule_one(
      const yellowstone::SubscriberID subscriber_id,
      const yellowstone::EventID event_id,
      py::object payload,
      const std::int64_t forward_at_unix_nanoseconds,
      const yellowstone::Superstep superstep
    )
    {
      m_event_bus.m_validate_payload(payload);

      return m_scheduler.schedule_one_at_unix_nanoseconds(
        subscriber_id,
        event_id,
        PythonPayload{hold_python_object(payload)},
        forward_at_unix_nanoseconds,
        superstep
      );
    }

    void start(void)
    {
      m_scheduler.start();
    }

    void stop(void) noexcept
    {
      m_scheduler.stop();
    }

    [[nodiscard]] bool running(void) const noexcept
    {
      return m_scheduler.running();
    }

    [[nodiscard]] std::size_t pending_events(void) const noexcept
    {
      return m_scheduler.pending_events();
    }

    [[nodiscard]] std::size_t scheduled_events(void) const noexcept
    {
      return m_scheduler.scheduled_events();
    }

    [[nodiscard]] std::size_t forwarded_events(void) const noexcept
    {
      return m_scheduler.forwarded_events();
    }

    [[nodiscard]] std::size_t failed_events(void) const noexcept
    {
      return m_scheduler.failed_events();
    }

    [[nodiscard]] std::int64_t retry_delay_nanoseconds(void) const noexcept
    {
      return m_scheduler.retry_delay().count();
    }

  private:
    static void m_report_error(
      const yellowstone::ScheduleID schedule_id,
      std::exception_ptr error
    ) noexcept
    {
      py::gil_scoped_acquire gil{};

      try
      {
        std::rethrow_exception(std::move(error));
      }
      catch (py::error_already_set& python_error)
      {
        python_error.discard_as_unraisable(
          "yellowstone scheduler"
        );
      }
      catch (const std::exception& native_error)
      {
        const std::string message =
          "Yellowstone scheduler request "
          + std::to_string(schedule_id)
          + " failed: "
          + native_error.what();

        PyErr_SetString(
          PyExc_RuntimeError,
          message.c_str()
        );
        PyErr_WriteUnraisable(Py_None);
        PyErr_Clear();
      }
      catch (...)
      {
        PyErr_SetString(
          PyExc_RuntimeError,
          "Yellowstone scheduler encountered an unknown asynchronous error"
        );
        PyErr_WriteUnraisable(Py_None);
        PyErr_Clear();
      }
    }

    PythonEventBus& m_event_bus;
    NativeScheduler m_scheduler;
  };


} // namespace

PYBIND11_MODULE(_yellowstone, module)
{
  module.doc() = "Standalone generic Yellowstone event bus and deadline scheduler.";

  module.attr("MAX_SUBSCRIBERS") = py::int_(
    yellowstone::EventBus<PythonPayload>::k_max_subscribers
  );

  py::class_<yellowstone::PublishResult>(module, "PublishResult")
    .def_readonly("delivered", &yellowstone::PublishResult::delivered)
    .def_readonly("coalesced", &yellowstone::PublishResult::coalesced)
    .def("__repr__", [](const yellowstone::PublishResult& result)
    {
      return "PublishResult(delivered=" + std::to_string(result.delivered)
        + ", coalesced=" + std::to_string(result.coalesced) + ")";
    });

  py::class_<PythonEvent>(module, "Event")
    .def_static(
      "__class_getitem__",
      &make_generic_alias<PythonEvent>,
      py::arg("parameter")
    )
    .def_property_readonly("id", [](const PythonEvent& event) { return event.id; })
    .def_property_readonly("superstep", [](const PythonEvent& event) { return event.superstep; })
    .def_property_readonly("sequence", [](const PythonEvent& event) { return event.sequence; })
    .def_property_readonly("payload", [](const PythonEvent& event)
    {
      return borrow_python_object(event.payload);
    });

  py::class_<PythonEventBus>(
    module,
    "EventBus",
    py::release_gil_before_calling_cpp_dtor()
  )
    .def_static(
      "__class_getitem__",
      &make_generic_alias<PythonEventBus>,
      py::arg("parameter")
    )
    .def(
      py::init<py::object, std::size_t, py::object>(),
      py::arg("payload_type"),
      py::kw_only(),
      py::arg("capacity") = 1024UL,
      py::arg("merge") = py::none()
    )
    .def("subscribe", &PythonEventBus::subscribe, py::arg("handler"))
    .def("unsubscribe", &PythonEventBus::unsubscribe, py::arg("subscriber_id"))
    .def(
      "publish",
      &PythonEventBus::publish,
      py::arg("event_id"),
      py::arg("payload"),
      py::kw_only(),
      py::arg("superstep") = 0UL
    )
    .def(
      "publish_one",
      &PythonEventBus::publish_one,
      py::arg("subscriber_id"),
      py::arg("event_id"),
      py::arg("payload"),
      py::kw_only(),
      py::arg("superstep") = 0UL
    )
    .def("start", &PythonEventBus::start, py::call_guard<py::gil_scoped_release>())
    .def("stop", &PythonEventBus::stop, py::call_guard<py::gil_scoped_release>())
    .def_property_readonly("running", &PythonEventBus::running)
    .def_property_readonly("subscriber_count", &PythonEventBus::subscriber_count)
    .def_property_readonly("capacity", &PythonEventBus::capacity)
    .def_property_readonly("payload_type", &PythonEventBus::payload_type)
    .def("pending", &PythonEventBus::pending, py::arg("subscriber_id"));

  py::class_<PythonScheduler>(
    module,
    "Scheduler",
    py::release_gil_before_calling_cpp_dtor()
  )
    .def_static(
      "__class_getitem__",
      &make_generic_alias<PythonScheduler>,
      py::arg("parameter")
    )
    .def(
      py::init<PythonEventBus&, std::int64_t>(),
      py::arg("event_bus"),
      py::kw_only(),
      py::arg("retry_delay_ns") = 1'000'000LL,
      py::keep_alive<1, 2>()
    )
    .def(
      "schedule",
      &PythonScheduler::schedule,
      py::arg("event_id"),
      py::arg("payload"),
      py::arg("forward_at_unix_nanoseconds"),
      py::kw_only(),
      py::arg("superstep") = 0UL
    )
    .def(
      "schedule_one",
      &PythonScheduler::schedule_one,
      py::arg("subscriber_id"),
      py::arg("event_id"),
      py::arg("payload"),
      py::arg("forward_at_unix_nanoseconds"),
      py::kw_only(),
      py::arg("superstep") = 0UL
    )
    .def(
      "start",
      &PythonScheduler::start,
      py::call_guard<py::gil_scoped_release>()
    )
    .def(
      "stop",
      &PythonScheduler::stop,
      py::call_guard<py::gil_scoped_release>()
    )
    .def_property_readonly(
      "running",
      &PythonScheduler::running
    )
    .def_property_readonly(
      "pending_events",
      &PythonScheduler::pending_events
    )
    .def_property_readonly(
      "scheduled_events",
      &PythonScheduler::scheduled_events
    )
    .def_property_readonly(
      "forwarded_events",
      &PythonScheduler::forwarded_events
    )
    .def_property_readonly(
      "failed_events",
      &PythonScheduler::failed_events
    )
    .def_property_readonly(
      "retry_delay_ns",
      &PythonScheduler::retry_delay_nanoseconds
    );
}
