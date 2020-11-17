import os
import numpy as np

from spn.structure.Base import Product, Sum
from spn.structure.leaves.histogram.Histograms import Histogram
from spn.structure.leaves.parametric.Parametric import Categorical
from spn.structure.Base import get_number_of_nodes,get_number_of_edges
from xspn.serialization.binary.BinarySerialization import BinarySerializer, BinaryDeserializer
from xspn.structure.Model import SPNModel
from xspn.structure.Query import JointProbability

def test_binary_serialization_roundtrip(tmpdir):
    """Tests the binary serialization for SPFlow SPNs by round-tripping 
    a simple SPN through serialization and de-serialization and comparing
    the graph-structure before and after serialization & de-serialization."""
    h1 = Histogram([0., 1., 2.], [0.25, 0.75], [1, 1], scope=1)
    h2 = Histogram([0., 1., 2.], [0.25, 0.75], [1, 1], scope=2)
    h3 = Histogram([0., 1., 2.], [0.25, 0.75], [1, 1], scope=1)
    h4 = Histogram([0., 1., 2.], [0.25, 0.75], [1, 1], scope=2)

    p0 = Product(children=[h1, h2])
    p1 = Product(children=[h3, h4])
    spn = Sum([0.3, 0.7], [p0, p1])

    model = SPNModel(spn)
    query = JointProbability(model)

    binary_file = os.path.join(tmpdir, "test.bin")
    print(f"Test binary file: {binary_file}")

    BinarySerializer(binary_file).serialize_to_file(query)

    deserialized = BinaryDeserializer(binary_file).deserialize_from_file()

    assert(isinstance(deserialized, JointProbability))
    assert(deserialized.batchSize == -1)
    assert(deserialized.rootError == 0.02)
    assert(deserialized.graph.featureType == model.featureType)
    assert(deserialized.graph.name == model.name)

    deserialized = deserialized.graph.root
    assert get_number_of_nodes(spn) == get_number_of_nodes(deserialized)
    assert get_number_of_nodes(spn, Sum) == get_number_of_nodes(deserialized, Sum)
    assert get_number_of_nodes(spn, Product) == get_number_of_nodes(deserialized, Product)
    assert get_number_of_nodes(spn, Histogram) == get_number_of_nodes(deserialized, Histogram)
    assert get_number_of_edges(spn) == get_number_of_edges(deserialized)


def test_categorical_leaf_serialization(tmpdir):
    """Tests the binary serialization of a single SPFlow Categorical leaf node
    by round-tripping and comparing the parameters before and after serialization
    & deserialization"""
    c = Categorical(p=[0.35, 0.55, 0.1], scope=1)

    binary_file = os.path.join(tmpdir, "test.bin")
    print(f"Test binary file: {binary_file}")

    model = SPNModel(c, "int8", "test")

    BinarySerializer(binary_file).serialize_to_file(model)

    deserialized = BinaryDeserializer(binary_file).deserialize_from_file()
    
    assert(isinstance(deserialized, SPNModel))
    assert(deserialized.featureType == model.featureType)
    assert(deserialized.name == model.name)
    
    deserialized = deserialized.root

    assert isinstance(deserialized, Categorical)
    assert len(c.p) == len(deserialized.p)
    for i,p in enumerate(c.p):
        assert p == deserialized.p[i]
    
