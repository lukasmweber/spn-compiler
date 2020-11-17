import logging
import os
import numpy as np
import capnp

from spn.algorithms.Validity import is_valid
from spn.structure.Base import Product, Sum, rebuild_scopes_bottom_up, assign_ids
from spn.structure.StatisticalTypes import Type, MetaType
from spn.structure.leaves.histogram.Histograms import Histogram
from spn.structure.leaves.parametric.Parametric import Gaussian, Categorical

from xspn.structure.Model import SPNModel
from xspn.structure.Query import Query, JointProbability

# Magic import making the schema defined in the schema language available
from  xspn.serialization.binary.capnproto import spflow_capnp

logger = logging.getLogger(__name__)

metaType2Enum = {MetaType.REAL : "real", MetaType.BINARY : "binary", MetaType.DISCRETE : "discrete"}

enum2MetaType = {v : k for k, v in metaType2Enum.items()}

type2Enum = {Type.REAL : "real", 
                Type.INTERVAL : "interval", 
                Type.POSITIVE : "positive", 
                Type.CATEGORICAL : "categorical",
                Type.ORDINAL : "ordinal", 
                Type.COUNT : "count", 
                Type.BINARY : "binary"}

enum2Type = {v : k for k, v in type2Enum.items()}

class BinarySerializer:
    """Interface to serialize SPNs from SPFlow into an efficient binary format."""

    def __init__(self, fileName, bufferSize = 10 * (2**10), clearFile = True):
        """Initialize the serializer.

        Keyword arguments:
        fileName -- The name of the output file.
        bufferSize -- Buffer size used during writing.
        clearFile -- If set to True (default), completely erase the file before writing.
        """
        self.assignedIDs = {}
        self.fileName = fileName
        self.bufferSize = bufferSize
        if clearFile:
            # Clear the content of the file if not instructed otherwise.
            open(fileName, "w").close()

    def serialize_to_file(self, content):
        content_msg = spflow_capnp.Content.new_message()
        if isinstance(content, SPNModel):
            content_msg.model = self._serialize_model(content)
            models = [content]
        elif isinstance(content, Query):
            content_msg.query = self._serialize_query(content)
            models = content.models()
        else:
            raise NotImplementedError(f"No serialization defined for content {content} of type {type(content)}")
        with open(self.fileName, "a+b", buffering=self.bufferSize*(2**10)) as outFile:
            content_msg.write(outFile)
            rootNodes = [model.root for model in models]
            self._serialize_graph(rootNodes, outFile)

    def _serialize_query(self, query):
        query_msg = spflow_capnp.Query.new_message()
        query_msg.batchSize = query.batchSize
        if isinstance(query, JointProbability):
            query_msg.joint = self._serialize_joint(query)
        else:
            raise NotImplementedError(f"No serialization defined for query {query} of type {type(query)}")
        return query_msg


    def _serialize_joint(self, joint):
        joint_msg = spflow_capnp.JointProbability.new_message()
        joint_msg.graph = self._serialize_model(joint.graph)
        joint_msg.relativeError = joint.rootError
        return joint_msg


    def _serialize_model(self, model):
        msg = spflow_capnp.Model.new_message()
        assert is_valid(model.root), "SPN invalid before serialization"
        # Assign (new) IDs to the nodes
        # Keep track of already assigned IDs, so the IDs are 
        # unique for the whole file.
        assign_ids(model.root, self.assignedIDs)
        # Rebuild scopes bottom-up
        rebuild_scopes_bottom_up(model.root)
        msg.rootNode = model.root.id
        msg.featureType = model.featureType
        name = ""
        if model.name is not None:
            name = model.name
        msg.name = name
        return msg


    def _serialize_graph(self, rootNodes, file):
        """Serialize SPN graphs to binary format. SPN graphs are given by their root node."""
        # Buffering write, buffer size was specified at initialization (defaults to 10 MiB).
        # The buffer size is specified in KiB during initialization, scale to bytes here.
        numNodes = 0
        for spn in rootNodes:
            visited = set()
            self._binary_serialize(spn, file, True, visited)
            numNodes += len(visited)
        print(f"Serialized {numNodes} nodes to {self.fileName}")

    def _binary_serialize(self, node, file, is_rootNode, visited_nodes):
        if node.id not in visited_nodes:
            if isinstance(node, Product):
                self._serialize_product(node, file, is_rootNode, visited_nodes)
            elif isinstance(node, Sum):
                self._serialize_sum(node, file, is_rootNode, visited_nodes)
            elif isinstance(node, Histogram):
                self._serialize_histogram(node, file, is_rootNode, visited_nodes)
            elif isinstance(node,Gaussian):
                self._serialize_gaussian(node, file, is_rootNode, visited_nodes)
            elif isinstance(node, Categorical):
                self._serialize_categorical(node, file, is_rootNode, visited_nodes)
            else:
                raise NotImplementedError(f"No serialization defined for node {node} of type {type(node)}")
            visited_nodes.add(node.id)

    def _serialize_product(self, product, file, is_rootNode, visited_nodes):
        # Serialize child nodes before node itself
        for c in product.children:
            self._binary_serialize(c, file, False, visited_nodes)
        # Construct inner product node message.
        prod_msg = spflow_capnp.ProductNode.new_message()
        children = prod_msg.init("children", len(product.children))
        for i, child in enumerate(product.children):
            children[i] = child.id
        # Construct surrounding node message
        node = spflow_capnp.Node.new_message()
        node.id = product.id
        node.product = prod_msg
        node.rootNode = is_rootNode
        node.write(file)

    def _serialize_sum(self, sum, file, is_rootNode, visited_nodes):
        # Serialize child nodes before node itself
        for c in sum.children:
            self._binary_serialize(c, file, False, visited_nodes)
        # Construct innner sum node message
        sum_msg = spflow_capnp.SumNode.new_message()
        children = sum_msg.init("children", len(sum.children))
        for i, child in enumerate(sum.children):
            children[i] = child.id
        weights = sum_msg.init("weights", len(sum.weights))
        for i, w in enumerate(sum.weights):
            weights[i] = BinarySerializer._unwrap_value(w)
        # Construct surrounding node message
        node = spflow_capnp.Node.new_message()
        node.id = sum.id
        node.sum = sum_msg
        node.rootNode = is_rootNode
        node.write(file)

    def _serialize_histogram(self, hist, file, is_rootNode, visited_nodes):
        # Construct inner histogram leaf message.
        hist_msg = spflow_capnp.HistogramLeaf.new_message()
        breaks = hist_msg.init("breaks", len(hist.breaks))
        for i,b in enumerate(hist.breaks):
            breaks[i] = int(b)
        densities = hist_msg.init("densities", len(hist.densities))
        for i,d in enumerate(hist.densities):
            densities[i] = BinarySerializer._unwrap_value(d)
        reprPoints = hist_msg.init("binReprPoints", len(hist.bin_repr_points))
        for i,r in enumerate(hist.bin_repr_points):
            reprPoints[i] = BinarySerializer._unwrap_value(r)
        hist_msg.type = type2Enum.get(hist.type)
        hist_msg.metaType = metaType2Enum.get(hist.meta_type)
        # Check that scope is defined over a single variable
        assert len(hist.scope) == 1, "Expecting Histogram to be univariate"
        hist_msg.scope = BinarySerializer._unwrap_value(hist.scope[0])
        # Construct surrounding node message.
        node = spflow_capnp.Node.new_message()
        node.hist = hist_msg
        node.rootNode = is_rootNode
        node.id = hist.id
        node.write(file)

    def _serialize_gaussian(self, gauss, file, is_rootNode, visited_nodes):
        # Construct inner Gaussian leaf message
        gauss_msg = spflow_capnp.GaussianLeaf.new_message()
        gauss_msg.mean = BinarySerializer._unwrap_value(gauss.mean)
        gauss_msg.stddev = BinarySerializer._unwrap_value(gauss.stdev)
        # Check that scope is defined over a single variable
        assert len(gauss.scope) == 1, "Expecting Gauss to be univariate"
        gauss_msg.scope = BinarySerializer._unwrap_value(gauss.scope[0])
        # Construct surrounding node message.
        node = spflow_capnp.Node.new_message()
        node.gaussian = gauss_msg
        node.rootNode = is_rootNode
        node.id = gauss.id
        node.write(file)

    def _serialize_categorical(self, categorical, file, is_rootNode, visited_nodes):
        # Construct inner categorical leaf message.
        cat_msg = spflow_capnp.CategoricalLeaf.new_message()
        probabilities = cat_msg.init("probabilities", len(categorical.p))
        for i,p in enumerate(categorical.p):
            probabilities[i] = BinarySerializer._unwrap_value(p)
        # Check that the scope is defined over a single variable
        assert len(categorical.scope) == 1, "Expecting Categorical leaf to be univariate"
        cat_msg.scope = BinarySerializer._unwrap_value(categorical.scope[0])
        node = spflow_capnp.Node.new_message()
        node.categorical = cat_msg
        node.rootNode = is_rootNode
        node.id = categorical.id
        node.write(file)
    
    @staticmethod
    def _unwrap_value(value):
        # If the value was defined in the module numpy, convert it to a
        # Python primitive type for serialization.
        if type(value).__module__ == np.__name__:
            return value.item()
        return value



class BinaryDeserializer:
    """Interface to de-serialize (read) SPNs from SPFlow from an efficient binary format."""

    def __init__(self, fileName):
        """Initialize the de-serializer."""
        self.fileName = fileName

    def deserialize_from_file(self):
        """Deserialize all SPN graphs from the file. Returns a list of SPN graph root nodes."""
        with open(self.fileName, "rb") as inFile:
            # Read header (Content) message first
            content = spflow_capnp.Content.read(inFile)
            # Read serialized SPN graph before reconstructing model & query.
            rootNodes = self._binary_deserialize_graph(inFile)
            for root in rootNodes:
                rebuild_scopes_bottom_up(root)
                assert is_valid(root), "SPN invalid after deserialization"

            which = content.which()
            if which == "model":
                deserialized = self._deserialize_model(content.model, rootNodes)
            elif which == "query":
                deserialized = self._deserialize_query(content.query, rootNodes)
            return deserialized


    def _deserialize_query(self, msg, rootNodes):
        model = self._deserialize_model(msg.joint.graph, rootNodes)
        relativeError = msg.joint.relativeError
        batchSize = msg.batchSize
        return JointProbability(model, batchSize, relativeError)

    def _deserialize_model(self, msg, rootNodes):
        rootID = msg.rootNode
        featureType = msg.featureType
        name = msg.name
        if name == "":
            name = None
        rootNode = next((root for root in rootNodes if root.id == rootID), None)
        if rootNode is None:
            logger.error(f"Did not find serialized root node {rootID}")
        return SPNModel(rootNode, featureType, name)


    def _binary_deserialize_graph(self, file):
        node_map = {}
        nodes = []
        for node in spflow_capnp.Node.read_multiple(file):
            which = node.which()
            deserialized = None
            if which == "product":
                deserialized = self._deserialize_product(node, node_map)
            elif which == "sum":
                deserialized = self._deserialize_sum(node, node_map)
            elif which == "hist":
                deserialized = self._deserialize_histogram(node, node_map)
            elif which == "gaussian":
                deserialized = self._deserialize_gaussian(node, node_map)
            elif which == "categorical":
                deserialized = self._deserialize_categorical(node, node_map)
            else:
                raise NotImplementedError(f"No deserialization defined for {which}")
            node_map[node.id] = deserialized
            if node.rootNode:
                nodes.append(deserialized)
        print(f"Deserialized {len(node_map)} from {file.name}")
        return nodes

    def _deserialize_product(self, node, node_map):
        child_ids = node.product.children
        # Resolve references to child nodes by ID.
        children = [node_map.get(id) for id in child_ids]
        # Check all childs have been resolved.
        assert None not in children, "Child node ID could not be resolved"
        product = Product(children = children)
        product.id = node.id
        return product

    def _deserialize_sum(self, node, node_map):
        child_ids = node.sum.children
        # Resolve references to child nodes by ID.
        children = [node_map.get(id) for id in child_ids]
        # Check all childs have been resolved.
        assert None not in children, "Child node ID could not be resolved"
        sum = Sum(children = children, weights=node.sum.weights)
        sum.id = node.id
        return sum

    def _deserialize_histogram(self, node, node_map):
        breaks = node.hist.breaks
        densities = node.hist.densities
        reprPoints = node.hist.binReprPoints
        type = enum2Type.get(node.hist.type)
        metaType = enum2MetaType.get(node.hist.metaType)
        hist = Histogram(breaks=breaks, densities=densities, bin_repr_points=reprPoints, scope=node.hist.scope,
                        type_=type, meta_type=metaType)
        hist.id = node.id
        return hist

    def _deserialize_gaussian(self, node, node_map):
        gauss = Gaussian(node.gaussian.mean, node.gaussian.stddev, node.gaussian.scope)
        gauss.id = node.id
        return gauss

    def _deserialize_categorical(self, node, node_map):
        probabilities = node.categorical.probabilities
        cat = Categorical(p=probabilities, scope=node.categorical.scope)
        cat.id = node.id
        return cat
