import arbor
import functools
import unittest
from pathlib import Path
import subprocess

_mpi_enabled = arbor.__config__["mpi"]
_mpi4py_enabled = arbor.__config__["mpi4py"]

def _fix(param_name, fixture, func):
    """
    Decorates `func` to inject the `fixture` callable result as `param_name`.
    """
    @functools.wraps(func)
    def wrapper(*args, **kwargs):
        kwargs[param_name] = fixture()
        return func(*args, **kwargs)

    return wrapper

def _fixture(decorator):
    @functools.wraps(decorator)
    def fixture_decorator(func):
        return _fix(decorator.__name__, decorator, func)

    return fixture_decorator

def _singleton_fixture(f):
    return _fixture(functools.lru_cache(f))


@_fixture
def repo_path():
    """
    Fixture that returns the repo root path.
    """
    return Path(__file__).parent.parent.parent

@_fixture
def context():
    args = [arbor.proc_allocation()]
    if _mpi_enabled:
        if not arbor.mpi_is_initialized():
            print("Context fixture initializing mpi", flush=True)
            arbor.mpi_init()
        if _mpi4py_enabled:
            from mpi4py.MPI import COMM_WORLD as comm
        else:
            comm = arbor.mpi_comm()
        args.append(comm)
    return arbor.context(*args)


@_singleton_fixture
@context
@repo_path
def dummy_catalogue(context, repo_path):
    curr = Path.cwd()
    from mpi4py.MPI import COMM_WORLD as comm
    build_err = None
    try:
        if not context.rank:
            path = repo_path / "test" / "unit" / "dummy"
            subprocess.run(["build-catalogue", "dummy", str(path)], check=True)
        build_err = comm.bcast(build_err, root=0)
    except Exception as e:
        build_err = comm.bcast(e, root=0)
    if build_err:
        raise RuntimeError("Tests can't build catalogues")

    return arbor.load_catalogue(str(curr / "dummy-catalogue.so"))
